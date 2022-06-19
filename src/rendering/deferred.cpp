#include "deferred.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <util/fs/fs_util.h>

// tmp
#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl.h>
#include <imgui/imgui_impl_vulkan.h>

namespace hyperion::v2 {

using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;

UIPass::UIPass()
    : FullScreenPass()
{
}

void UIPass::Create(Engine *engine)
{
    /* will only init once */
    full_screen_quad->Init(engine);

    CreateRenderPass(engine);

    m_framebuffer = engine->resources.framebuffers.Add(std::make_unique<Framebuffer>(
        engine->GetInstance()->swapchain->extent,
        m_render_pass.IncRef()
    ));

    /* Add all attachments from the renderpass */
    for (auto *attachment_ref : m_render_pass->GetRenderPass().GetAttachmentRefs()) {
        m_framebuffer->GetFramebuffer().AddAttachmentRef(attachment_ref);
    }
    
    m_framebuffer.Init();
    
    HYP_FLUSH_RENDER_QUEUE(engine);
}

void UIPass::CreateRenderPass(Engine *engine)
{
    /* Add the filters' renderpass */
    auto render_pass = std::make_unique<RenderPass>(
        renderer::RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_INLINE
    );

    renderer::AttachmentRef *attachment_ref;

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            engine->GetInstance()->swapchain->extent,
            engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_COLOR),
            nullptr
        ),
        renderer::RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
        engine->GetInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetInstance()->GetDevice()));
    }

    m_render_pass = engine->resources.render_passes.Add(std::move(render_pass));
    m_render_pass.Init();
}

void UIPass::Destroy(Engine *engine)
{
    m_render_pass = nullptr;
    m_framebuffer = nullptr;

    engine->render_scheduler.Enqueue([this, engine, &frame_data = *m_frame_data](...) {
        auto result = renderer::Result::OK;

        for (auto &attachment : m_attachments) {
            HYPERION_PASS_ERRORS(attachment->Destroy(engine->GetInstance()->GetDevice()), result);
        }

        m_attachments.clear();

        return result;
    });

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void UIPass::Render(Engine *engine, CommandBuffer *primary)
{

    { bool show_another_window = true;
        ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            show_another_window = false;
        ImGui::End();
    }


    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();

    // Record dear imgui primitives into command buffer
    m_framebuffer->BeginCapture(primary);
    ImGui_ImplVulkan_RenderDrawData(draw_data, primary->GetCommandBuffer());
    m_framebuffer->EndCapture(primary);
}

DeferredPass::DeferredPass()
    : FullScreenPass()
{
    
}

DeferredPass::~DeferredPass() = default;

void DeferredPass::CreateShader(Engine *engine)
{
    m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(
        std::vector<SubShader>{
            SubShader{ShaderModule::Type::VERTEX, {
                FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred_vert.spv")).Read(),
                {.name = "deferred vert"}
            }},
            SubShader{ShaderModule::Type::FRAGMENT, {
                FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred_frag.spv")).Read(),
                {.name = "deferred frag"}
            }}
        }
    ));

    m_shader->Init(engine);
}

void DeferredPass::CreateRenderPass(Engine *engine)
{
    m_render_pass = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].GetRenderPass().IncRef();
}

void DeferredPass::CreateDescriptors(Engine *engine)
{
    // TODO: this could cause invalidation issues, refactor this class into an EngineComponent and use AttachCallback
    //engine->callbacks.Once(EngineCallback::CREATE_DESCRIPTOR_SETS, [this, engine](...) {
    //engine->render_scheduler.Enqueue([this, engine, &framebuffer = m_framebuffer->GetFramebuffer()](...) {
        auto &framebuffer = m_framebuffer->GetFramebuffer();
        if (!framebuffer.GetAttachmentRefs().empty()) {
            auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL);
            auto *descriptor = descriptor_set->GetOrAddDescriptor<ImageSamplerDescriptor>(DescriptorKey::DEFERRED_RESULT);

            for (auto *attachment_ref : framebuffer.GetAttachmentRefs()) {
                descriptor->SetSubDescriptor({
                    .element_index = ~0u,
                    .image_view    = attachment_ref->GetImageView(),
                    .sampler       = attachment_ref->GetSampler()
                });
            }
        }
    //});

    //    HYPERION_RETURN_OK;
    //});
}

void DeferredPass::Create(Engine *engine)
{
    m_framebuffer = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffers()[0].IncRef();

    CreatePerFrameData(engine);
    CreatePipeline(engine);
}

void DeferredPass::Destroy(Engine *engine)
{
    FullScreenPass::Destroy(engine); // flushes render queue
}

void DeferredPass::Render(Engine *engine, Frame *frame)
{
}

DeferredRenderer::DeferredRenderer() = default;
DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Create(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    using renderer::ImageSamplerDescriptor;

    m_post_processing.Create(engine);

    m_pass.CreateShader(engine);
    m_pass.CreateRenderPass(engine);
    m_pass.Create(engine);

    m_ui_pass.Create(engine);

    auto &opaque_fbo = engine->GetRenderListContainer()[Bucket::BUCKET_OPAQUE].GetFramebuffers()[0];

    /* Add our gbuffer textures */
    auto *descriptor_set_pass = engine->GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL);

    /* Albedo texture */
    descriptor_set_pass
        ->AddDescriptor<ImageSamplerDescriptor>(0)
        ->SetSubDescriptor({
            .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[0]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[0]->GetSampler()
        });

    /* Normals texture*/
    descriptor_set_pass
        ->GetDescriptor(0)
        ->SetSubDescriptor({
            .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[1]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[1]->GetSampler()
        });

    /* Position texture */
    descriptor_set_pass
        ->GetDescriptor(0)
        ->SetSubDescriptor({
            .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[2]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[2]->GetSampler()
        });

    /* Material ID */
    descriptor_set_pass
        ->GetDescriptor(0)
        ->SetSubDescriptor({
            .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[3]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[3]->GetSampler()
        });

    /* Depth texture */
    descriptor_set_pass
        ->AddDescriptor<ImageSamplerDescriptor>(1)
        ->SetSubDescriptor({
            .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[4]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[4]->GetSampler()
        });
    
    m_pass.CreateDescriptors(engine);

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void DeferredRenderer::Destroy(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_post_processing.Destroy(engine);
    m_pass.Destroy(engine);  // flushes render queue
}

void DeferredRenderer::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);


    m_ui_pass.Render(engine, frame->GetCommandBuffer());
    return;

    auto *primary = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    m_pass.Record(engine, frame_index);

    auto &render_list = engine->GetRenderListContainer();
    auto &bucket = render_list.Get(BUCKET_OPAQUE);
    
    // begin opaque objs
    bucket.GetFramebuffers()[0]->BeginCapture(primary); /* TODO: frame index? */
    RenderOpaqueObjects(engine, frame);
    bucket.GetFramebuffers()[0]->EndCapture(primary); /* TODO: frame index? */
    // begin opaque objs
    
    m_post_processing.RenderPre(engine, frame);

    // begin translucent objs
    m_pass.GetFramebuffer()->BeginCapture(primary);
    /* Render deferred shading onto full screen quad */
    HYPERION_ASSERT_RESULT(m_pass.GetFrameData()->At(frame_index).Get<CommandBuffer>()->SubmitSecondary(primary));

    RenderTranslucentObjects(engine, frame);

    m_pass.GetFramebuffer()->EndCapture(primary);
    // end translucent objs


    m_post_processing.RenderPost(engine, frame);
}

void DeferredRenderer::RenderOpaqueObjects(Engine *engine, Frame *frame)
{
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_SKYBOX).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
    
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
}

void DeferredRenderer::RenderTranslucentObjects(Engine *engine, Frame *frame)
{
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_TRANSLUCENT).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
}

} // namespace hyperion::v2
