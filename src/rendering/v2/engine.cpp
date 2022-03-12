#include "engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>

#include <rendering/backend/renderer_helpers.h>

#include "rendering/mesh.h"

namespace hyperion::v2 {

using renderer::MeshInputAttribute;
using renderer::MeshInputAttributeSet;
using renderer::AttachmentBase;
using renderer::Attachment;
using renderer::ImageView;
using renderer::FramebufferObject;

Engine::Engine(SystemSDL &_system, const char *app_name)
    : m_instance(new Instance(_system, app_name, "HyperionEngine"))
{
}

Engine::~Engine()
{
    m_filter_stack.Destroy(this);

    // TODO: refactor
    //m_swapchain_data.shader->Destroy(this);

    for (auto &it : m_framebuffers) {
        it->Destroy(this);
    }

    for (auto &it : m_render_passes) {
        it->Destroy(this);
    }

    for (auto &shader : m_shaders) {
        shader->Destroy(this);
    }

    m_instance->Destroy();
}

Shader::ID Engine::AddShader(std::unique_ptr<Shader> &&shader)
{
    AssertThrow(shader != nullptr);

    Shader::ID id(m_shaders.size());

    shader->Create(this);

    m_shaders.push_back(std::move(shader));

    return id;
}

Framebuffer::ID Engine::AddFramebuffer(std::unique_ptr<Framebuffer> &&framebuffer, RenderPass::ID render_pass)
{
    AssertThrow(framebuffer != nullptr);

    Framebuffer::ID id(m_framebuffers.size());
    framebuffer->Create(this, GetRenderPass(render_pass));

    m_framebuffers.push_back(std::move(framebuffer));

    return id;
}

Framebuffer::ID Engine::AddFramebuffer(size_t width, size_t height, RenderPass::ID render_pass_id)
{
    RenderPass *render_pass = GetRenderPass(render_pass_id);

    AssertThrow(render_pass != nullptr);

    auto framebuffer = std::make_unique<Framebuffer>(width, height);

    /* Add all attachments from the renderpass */
    for (auto &it : render_pass->GetAttachments()) {
        framebuffer->GetWrappedObject()->AddAttachment(it.format);
    }

    return AddFramebuffer(std::move(framebuffer), render_pass_id);
}

RenderPass::ID Engine::AddRenderPass(std::unique_ptr<RenderPass> &&render_pass)
{
    AssertThrow(render_pass != nullptr);

    RenderPass::ID id(m_render_passes.size());
    render_pass->Create(this);

    m_render_passes.push_back(std::move(render_pass));

    return id;
}

void Engine::AddPipeline(Pipeline::Builder &&builder, Pipeline **out)
{
    auto *shader = GetShader(builder.m_construction_info.shader_id);

    AssertThrow(shader != nullptr);

    builder.m_construction_info.shader = non_owning_ptr(shader->GetWrappedObject());

    auto *render_pass = GetRenderPass(builder.m_construction_info.render_pass_id);
    AssertThrow(render_pass != nullptr);
    // TODO: Assert that render_pass matches the layout of what the fbo was set up with

    builder.m_construction_info.render_pass = non_owning_ptr(render_pass->GetWrappedObject());

    for (int fbo_id : builder.m_construction_info.fbo_ids) {
        if (auto fbo = GetFramebuffer(fbo_id)) {
            builder.m_construction_info.fbos.push_back(non_owning_ptr(fbo->GetWrappedObject()));
        }
    }

    auto add_pipeline_result = m_instance->AddPipeline(std::move(builder), out);
    AssertThrowMsg(add_pipeline_result, "%s", add_pipeline_result.message);
}


void Engine::InitializeInstance()
{
    auto renderer_initialize_result = m_instance->Initialize(true);
    AssertThrowMsg(renderer_initialize_result, "%s", renderer_initialize_result.message);
}

void Engine::FindTextureFormatDefaults()
{
    const Device *device = m_instance->GetDevice();

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_COLOR,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
                        Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16,
                        Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_DEPTH,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16,
                        Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_GBUFFER,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );
}

void Engine::PrepareSwapchain()
{

    m_filter_stack.Create(this);


    // TODO: should be moved elsewhere. SPIR-V for rendering quad could be static
    {
        auto blit_shader = std::make_unique<Shader>(std::vector{
            SpirvObject{ SpirvObject::Type::VERTEX, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/blit_vert.spv").Read() },
            SpirvObject{ SpirvObject::Type::FRAGMENT, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/blit_frag.spv").Read() }
        });

        m_swapchain_data.shader_id = AddShader(std::move(blit_shader));
    }
    

    // TMP trying to update a descriptor set right on 
    const auto vertex_attributes = MeshInputAttributeSet(
        MeshInputAttribute::MESH_INPUT_ATTRIBUTE_POSITION
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TANGENT
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT);


    auto render_pass = std::make_unique<RenderPass>(RenderPass::RENDER_PASS_STAGE_PRESENT, RenderPass::RENDER_PASS_INLINE);
    /* For our color attachment */
    render_pass->GetWrappedObject()->AddAttachment(renderer::RenderPass::AttachmentInfo{
        .attachment = std::make_unique<Attachment<VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR>>
            (0, m_instance->swapchain->image_format),
        .is_depth_attachment = false
    });

    render_pass->AddAttachment({
        .format = m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_DEPTH)
    });
    
    RenderPass::ID render_pass_id = AddRenderPass(std::move(render_pass));

    Pipeline::Builder builder;
    builder
        .Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) /* full screen quad is a triangle fan */
        .Shader(m_swapchain_data.shader_id)
        .VertexAttributes(vertex_attributes)
        .RenderPass(render_pass_id);

    for (auto img : m_instance->swapchain->images) {
        auto image_view = std::make_unique<ImageView>(VK_IMAGE_ASPECT_COLOR_BIT);

        /* Create imageview independent of a Image */
        auto image_view_result = image_view->Create(
            m_instance->GetDevice(),
            img,
            m_instance->swapchain->image_format,
            VK_IMAGE_VIEW_TYPE_2D
        );

        AssertThrowMsg(image_view_result, "%s", image_view_result.message);

        auto fbo = std::make_unique<Framebuffer>(m_instance->swapchain->extent.width, m_instance->swapchain->extent.height);
        fbo->GetWrappedObject()->AddAttachment(
            FramebufferObject::AttachmentImageInfo{
                .image = nullptr,
                .image_view = std::move(image_view),
                .sampler = nullptr,
                .image_needs_creation = false,
                .image_view_needs_creation = false,
                .sampler_needs_creation = true
            },
            m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_COLOR) // unused but will tell the fbo that it is not a depth texture
        );

        /* Now we add a depth buffer */
        auto result = fbo->GetWrappedObject()->AddAttachment(m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_DEPTH));
        AssertThrowMsg(result, "%s", result.message);

        builder.Framebuffer(AddFramebuffer(std::move(fbo), render_pass_id));
    }

    AddPipeline(std::move(builder), &m_swapchain_data.pipeline);
}

void Engine::Initialize()
{
    InitializeInstance();
    FindTextureFormatDefaults();
}

void Engine::RenderPostProcessing(Frame *frame, uint32_t frame_index)
{
    m_filter_stack.Render(this, frame, frame_index);
}

void Engine::RenderSwapchain(Frame *frame)
{
    m_swapchain_data.pipeline->Bind(frame->command_buffer);

    m_instance->GetDescriptorPool().BindDescriptorSets(frame->command_buffer, m_swapchain_data.pipeline->layout);

    Filter::full_screen_quad->RenderVk(frame, m_instance.get(), nullptr);
}
} // namespace hyperion::v2