#include "engine.h"

#include <asset/byte_reader.h>
#include <util/fs/fs_util.h>

#include <rendering/post_fx.h>
#include <rendering/compute.h>
#include <rendering/environment.h>

#include <rendering/backend/renderer_features.h>

#include <audio/audio_manager.h>

namespace hyperion::v2 {

using renderer::VertexAttributeSet;
using renderer::Attachment;
using renderer::ImageView;
using renderer::FramebufferObject;
using renderer::DescriptorKey;
using renderer::FillMode;

Engine::Engine(SystemSDL &_system, const char *app_name)
    : shader_globals(nullptr),
      m_instance(new Instance(_system, app_name, "HyperionEngine")),
      resources(this),
      assets(this),
      terrain_thread(Threads::thread_ids.At(THREAD_TERRAIN), 30.0f)
{
}

Engine::~Engine()
{

    //game_thread.Join(); // stop looping in game thread
    
    callbacks.Trigger(EngineCallback::DESTROY_ANY, this);
    callbacks.Trigger(EngineCallback::DESTROY_ACCELERATION_STRUCTURES, this);
    callbacks.Trigger(EngineCallback::DESTROY_MESHES, this);
    callbacks.Trigger(EngineCallback::DESTROY_MATERIALS, this);
    callbacks.Trigger(EngineCallback::DESTROY_LIGHTS, this);
    callbacks.Trigger(EngineCallback::DESTROY_SKELETONS, this);
    callbacks.Trigger(EngineCallback::DESTROY_SPATIALS, this);
    callbacks.Trigger(EngineCallback::DESTROY_SHADERS, this);
    callbacks.Trigger(EngineCallback::DESTROY_TEXTURES, this);
    callbacks.Trigger(EngineCallback::DESTROY_VOXELIZER, this);
    callbacks.Trigger(EngineCallback::DESTROY_DESCRIPTOR_SETS, this);
    callbacks.Trigger(EngineCallback::DESTROY_GRAPHICS_PIPELINES, this);
    callbacks.Trigger(EngineCallback::DESTROY_COMPUTE_PIPELINES, this);
    callbacks.Trigger(EngineCallback::DESTROY_RAYTRACING_PIPELINES, this);
    callbacks.Trigger(EngineCallback::DESTROY_SCENES, this);
    callbacks.Trigger(EngineCallback::DESTROY_ENVIRONMENTS, this);
    callbacks.Trigger(EngineCallback::DESTROY_FRAMEBUFFERS, this);
    callbacks.Trigger(EngineCallback::DESTROY_RENDER_PASSES, this);

    m_dummy_data.Destroy(this);

    HYP_FLUSH_RENDER_QUEUE(this); // just to clear anything remaining up 

    AssertThrow(m_instance != nullptr);
    (void)m_instance->GetDevice()->Wait();
    
    m_render_list_container.Destroy(this);
    
    m_deferred_renderer.Destroy(this);

    for (auto &attachment : m_render_pass_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Destroy(m_instance->GetDevice()));
    }
    
    resources.Destroy(this);

    if (shader_globals != nullptr) {
        shader_globals->Destroy(this);

        delete shader_globals;
    }

    m_instance->Destroy();
}

void Engine::FindTextureFormatDefaults()
{
    Threads::AssertOnThread(THREAD_RENDER);

    const Device *device = m_instance->GetDevice();

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_COLOR,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_BGRA8_SRGB,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8 },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_DEPTH,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_24,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_GBUFFER,
        device->GetFeatures().FindSupportedFormat(
            std::array{ //Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R10G10B10A2,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_GBUFFER_8BIT,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8 },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_NORMALS,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_UV,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RG32F},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_STORAGE,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
        )
    );
}

void Engine::PrepareSwapchain()
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto shader = resources.shaders.Add(std::make_unique<Shader>(
        std::vector<SubShader>{
            {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(assets.GetBasePath(), "vkshaders/blit_vert.spv")).Read()}},
            {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(assets.GetBasePath(), "vkshaders/blit_frag.spv")).Read()}}
        }
    ));

    shader->Init(this);

    UInt iteration = 0;
    
    auto render_pass = resources.render_passes.Add(std::make_unique<RenderPass>(
        renderer::RenderPassStage::PRESENT,
        renderer::RenderPass::Mode::RENDER_PASS_INLINE
    ));

    m_render_pass_attachments.push_back(std::make_unique<renderer::Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            m_instance->swapchain->extent,
            m_instance->swapchain->image_format,
            nullptr
        ),
        renderer::RenderPassStage::PRESENT
    ));

    m_render_pass_attachments.push_back(std::make_unique<renderer::Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            m_instance->swapchain->extent,
            m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        renderer::RenderPassStage::PRESENT
    ));
    
    for (auto &attachment : m_render_pass_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(m_instance->GetDevice()));
    }

    for (VkImage img : m_instance->swapchain->images) {
        auto fbo = std::make_unique<Framebuffer>(
            m_instance->swapchain->extent,
            render_pass.IncRef()
        );

        renderer::AttachmentRef *color_attachment_ref,
                                *depth_attachment_ref;

        HYPERION_ASSERT_RESULT(m_render_pass_attachments[0]->AddAttachmentRef(
            m_instance->GetDevice(),
            img,
            renderer::Image::ToVkFormat(m_instance->swapchain->image_format),
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D,
            1, 1,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &color_attachment_ref
        ));

        color_attachment_ref->SetBinding(0);

        fbo->GetFramebuffer().AddAttachmentRef(color_attachment_ref);

        HYPERION_ASSERT_RESULT(m_render_pass_attachments[1]->AddAttachmentRef(
            m_instance->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &depth_attachment_ref
        ));

        fbo->GetFramebuffer().AddAttachmentRef(depth_attachment_ref);

        depth_attachment_ref->SetBinding(1);

        if (iteration == 0) {
            render_pass->GetRenderPass().AddAttachmentRef(color_attachment_ref);
            render_pass->GetRenderPass().AddAttachmentRef(depth_attachment_ref);

            render_pass.Init();

            m_root_pipeline = std::make_unique<GraphicsPipeline>(
                shader.IncRef(),
                render_pass.IncRef(),
                RenderableAttributeSet{
                    .bucket            = BUCKET_SWAPCHAIN,
                    .vertex_attributes = renderer::static_mesh_vertex_attributes,
                    .fill_mode         = FillMode::FILL  
                }
            );
        }

        m_root_pipeline->AddFramebuffer(resources.framebuffers.Add(std::move(fbo)));

        ++iteration;
    }
    
    //m_root_pipeline->SetFaceCullMode(FaceCullMode::FRONT);

    callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this](...) {
        m_render_list_container.AddFramebuffersToPipelines(this);
        m_root_pipeline->Init(this);
    });
}

void Engine::Initialize()
{
    Threads::AssertOnThread(THREAD_RENDER);

    HYPERION_ASSERT_RESULT(m_instance->Initialize(true));

    FindTextureFormatDefaults();

    shader_globals = new ShaderGlobals(m_instance->GetFrameHandler()->NumFrames());
    shader_globals->Create(this);

    m_dummy_data.Create(this);

    m_world.Init(this);
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->SetSubDescriptor({
            .buffer = shader_globals->scenes.GetBuffers()[0].get(),
            .range = static_cast<UInt>(sizeof(SceneShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
        ->SetSubDescriptor({
            .buffer = shader_globals->lights.GetBuffers()[0].get(),
            .range  = static_cast<UInt>(sizeof(LightShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->GetOrAddDescriptor<renderer::ImageSamplerDescriptor>(DescriptorKey::SHADOW_MAPS);

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::SHADOW_MATRICES)
        ->SetSubDescriptor({ .buffer = shader_globals->shadow_maps.GetBuffers()[0].get() });
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->SetSubDescriptor({
            .buffer = shader_globals->materials.GetBuffers()[0].get(),
            .range = static_cast<UInt>(sizeof(MaterialShaderData))
        });


    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
        ->SetSubDescriptor({
            .buffer = shader_globals->objects.GetBuffers()[0].get(),
            .range = static_cast<UInt>(sizeof(ObjectShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(2)
        ->SetSubDescriptor({
            .buffer = shader_globals->skeletons.GetBuffers()[0].get(),
            .range = static_cast<UInt>(sizeof(SkeletonShaderData))
        });


    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->SetSubDescriptor({
            .buffer = shader_globals->scenes.GetBuffers()[1].get(),
            .range = static_cast<UInt>(sizeof(SceneShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
        ->SetSubDescriptor({
            .buffer = shader_globals->lights.GetBuffers()[1].get(),
            .range  = static_cast<UInt>(sizeof(LightShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->GetOrAddDescriptor<renderer::ImageSamplerDescriptor>(DescriptorKey::SHADOW_MAPS);

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::SHADOW_MATRICES)
        ->SetSubDescriptor({ .buffer = shader_globals->shadow_maps.GetBuffers()[1].get() });
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->SetSubDescriptor({
            .buffer = shader_globals->materials.GetBuffers()[1].get(),
            .range = static_cast<UInt>(sizeof(MaterialShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
        ->SetSubDescriptor({
            .buffer = shader_globals->objects.GetBuffers()[1].get(),
            .range = static_cast<UInt>(sizeof(ObjectShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(2)
        ->SetSubDescriptor({
            .buffer = shader_globals->skeletons.GetBuffers()[1].get(),
            .range = static_cast<UInt>(sizeof(SkeletonShaderData))
        });

#if HYP_FEATURES_BINDLESS_TEXTURES
    m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS)
        ->AddDescriptor<renderer::ImageSamplerDescriptor>(0);

    m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1)
        ->AddDescriptor<renderer::ImageSamplerDescriptor>(0);
#else
    auto *material_sampler_descriptor = m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES)
        ->AddDescriptor<renderer::SamplerDescriptor>(renderer::DescriptorKey::SAMPLER);

    material_sampler_descriptor->SetSubDescriptor({
        .sampler = &GetDummyData().GetSampler()
    });

    auto *material_textures_descriptor = m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES)
        ->AddDescriptor<renderer::ImageDescriptor>(renderer::DescriptorKey::TEXTURES);

    for (UInt i = 0; i < DescriptorSet::max_material_texture_samplers; i++) {
        material_textures_descriptor->SetSubDescriptor({
            .element_index = i,
            .image_view    = &GetDummyData().GetImageView2D1x1R8()
        });
    }
#endif

    for (DescriptorSet::Index descriptor_set_index : DescriptorSet::global_buffer_mapping) {
        auto *descriptor_set = GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(descriptor_set_index);

        descriptor_set
            ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::CUBEMAP_UNIFORMS)
            ->SetSubDescriptor({
                .element_index = 0,
                .buffer        = &shader_globals->cubemap_uniforms
            });

        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::CUBEMAP_TEST)
            ->SetSubDescriptor({
                .element_index = 0,
                .image_view    = &GetDummyData().GetImageViewCube1x1R8()
            });

        descriptor_set
            ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::ENV_PROBES)
            ->SetSubDescriptor({
                .element_index = 0,
                .buffer        = &shader_globals->env_probes
            });
    }

    // add placeholder shadowmaps
    for (DescriptorSet::Index descriptor_set_index : DescriptorSet::scene_buffer_mapping) {
        auto *descriptor_set = GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(descriptor_set_index);

        auto *shadow_map_descriptor = descriptor_set
            ->GetOrAddDescriptor<renderer::ImageSamplerDescriptor>(DescriptorKey::SHADOW_MAPS);
        
        for (UInt i = 0; i < Environment::max_shadow_maps; i++) {
            shadow_map_descriptor->SetSubDescriptor({
                .element_index = i,
                .image_view    = &GetDummyData().GetImageView2D1x1R8(),
                .sampler       = &GetDummyData().GetSampler()
            });
        }
    }

    /* for textures */
    //shader_globals->textures.Create(this);
    
    callbacks.TriggerPersisted(EngineCallback::CREATE_ANY, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_RENDER_PASSES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_FRAMEBUFFERS, this);

    m_render_list_container.Create(this);
    
    callbacks.TriggerPersisted(EngineCallback::CREATE_ENVIRONMENTS, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_SCENES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_TEXTURES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_SHADERS, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_SPATIALS, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_MESHES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_ACCELERATION_STRUCTURES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_SKELETONS, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_LIGHTS, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_MATERIALS, this);

    AssertThrowMsg(AudioManager::GetInstance()->Initialize(), "Failed to initialize audio device");

    m_running = true;
}

void Engine::Compile()
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_deferred_renderer.Create(this);

    for (UInt i = 0; i < m_instance->GetFrameHandler()->NumFrames(); i++) {
        /* Finalize shadow maps */
        shader_globals->shadow_maps.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize lights */
        shader_globals->lights.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize skeletons */
        shader_globals->skeletons.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize materials */
        shader_globals->materials.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize per-object data */
        shader_globals->objects.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize scene data */
        shader_globals->scenes.UpdateBuffer(m_instance->GetDevice(), i);
    }

    callbacks.TriggerPersisted(EngineCallback::CREATE_VOXELIZER, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_DESCRIPTOR_SETS, this);

    /* Flush render queue before finalizing descriptors */
    HYP_FLUSH_RENDER_QUEUE(this);

    /* Finalize descriptor pool */
    HYPERION_ASSERT_RESULT(m_instance->GetDescriptorPool().Create(m_instance->GetDevice()));
    DebugLog(
        LogType::Debug,
        "Finalized descriptor pool\n"
    );
    
    callbacks.TriggerPersisted(EngineCallback::CREATE_GRAPHICS_PIPELINES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_COMPUTE_PIPELINES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_RAYTRACING_PIPELINES, this);

    AssertThrow(terrain_thread.Start());
}

Ref<GraphicsPipeline> Engine::FindOrCreateGraphicsPipeline(const RenderableAttributeSet &renderable_attributes)
{
    const auto it = m_graphics_pipeline_mapping.Find(renderable_attributes);

    if (it != m_graphics_pipeline_mapping.End()) {
        return resources.graphics_pipelines.Lookup(it->second);
    }

    auto &render_list_bucket = m_render_list_container.Get(renderable_attributes.bucket);

    auto shader = resources.shaders.Lookup(renderable_attributes.shader_id);
    AssertThrow(shader != nullptr);

    // create a pipeline with the given params
    return AddGraphicsPipeline(std::make_unique<GraphicsPipeline>(
        std::move(shader),
        render_list_bucket.GetRenderPass().IncRef(),
        renderable_attributes
    ));
}
    
Ref<GraphicsPipeline> Engine::AddGraphicsPipeline(std::unique_ptr<GraphicsPipeline> &&pipeline)
{
    auto graphics_pipeline = resources.graphics_pipelines.Add(std::move(pipeline));
    
    std::pair<RenderableAttributeSet, GraphicsPipeline::ID> pair{graphics_pipeline->GetRenderableAttributes(), graphics_pipeline->GetId()};
    m_graphics_pipeline_mapping.Insert(std::move(pair));

    m_render_list_container
        .Get(graphics_pipeline->GetRenderableAttributes().bucket)
        .AddGraphicsPipeline(graphics_pipeline.IncRef());

    return graphics_pipeline;
}

void Engine::ResetRenderState()
{
    Threads::AssertOnThread(THREAD_RENDER);

    render_state.scene_ids = {};
}

void Engine::UpdateBuffersAndDescriptors(UInt frame_index)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (auto deletion_flags = m_renderable_deletion_flag.load()) {
        std::lock_guard guard(m_renderable_deletion_mutex);

        if (deletion_flags & RENDERABLE_DELETION_TEXTURES) {
            if (PerformEnqueuedDeletions<Texture>()) {
                deletion_flags &= ~RENDERABLE_DELETION_TEXTURES;
            }
        }

        //if (deletion_flags & RENDERABLE_DELETION_MATERIALS) {
        //    if (PerformEnqueuedDeletions<Material>()) {
        //        deletion_flags &= ~RENDERABLE_DELETION_MATERIALS;
        //    }
        //}

        if (deletion_flags & RENDERABLE_DELETION_MESHES) {
            if (PerformEnqueuedDeletions<Mesh>()) {
                deletion_flags &= ~RENDERABLE_DELETION_MESHES;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_SKELETONS) {
            if (PerformEnqueuedDeletions<Skeleton>()) {
                deletion_flags &= ~RENDERABLE_DELETION_SKELETONS;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_SHADERS) {
            if (PerformEnqueuedDeletions<Shader>()) {
                deletion_flags &= ~RENDERABLE_DELETION_SHADERS;
            }
        }

        m_renderable_deletion_flag.store(deletion_flags);
    }

    shader_globals->scenes.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->objects.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->materials.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->skeletons.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->lights.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->shadow_maps.UpdateBuffer(m_instance->GetDevice(), frame_index);

    m_instance->GetDescriptorPool().DestroyPendingDescriptorSets(m_instance->GetDevice(), frame_index);
    m_instance->GetDescriptorPool().UpdateDescriptorSets(m_instance->GetDevice(), frame_index);
}

void Engine::RenderDeferred(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_deferred_renderer.Render(this, frame);
}

void Engine::RenderFinalPass(Frame *frame) const
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *pipeline                  = m_root_pipeline->GetPipeline();
    const UInt acquired_image_index = m_instance->GetFrameHandler()->GetAcquiredImageIndex();

    m_root_pipeline->GetFramebuffers()[acquired_image_index]->BeginCapture(frame->GetCommandBuffer());
    
    pipeline->Bind(frame->GetCommandBuffer());

    m_instance->GetDescriptorPool().Bind(
        m_instance->GetDevice(),
        frame->GetCommandBuffer(),
        pipeline,
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

#if HYP_FEATURES_ENABLE_RAYTRACING
    /* TMP */
    m_instance->GetDescriptorPool().Bind(
        m_instance->GetDevice(),
        command_buffer,
        pipeline,
        {{
            .set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
            .count = 1
        }}
    );
#endif

    /* Render full screen quad overlay to blit deferred + all post fx onto screen. */
    FullScreenPass::full_screen_quad->Render(const_cast<Engine *>(this), frame->GetCommandBuffer());
    
    m_root_pipeline->GetFramebuffers()[acquired_image_index]->EndCapture(frame->GetCommandBuffer());
}
} // namespace hyperion::v2
