#include "renderer.h"
#include "shader_manager.h"
#include "environment.h"
#include "../scene/scene_manager.h"
#include "postprocess/filters/deferred_rendering_filter.h"

#include "../util.h"

namespace hyperion {
Renderer::Renderer(const RenderWindow &render_window)
    : m_render_window(render_window),
      m_fbo(nullptr),
      m_environment(Environment::GetInstance()),
      m_deferred_pipeline(new DeferredPipeline())
{
    m_buckets[Spatial::Bucket::RB_SKY].enable_culling = false;
    m_buckets[Spatial::Bucket::RB_PARTICLE].enable_culling = false; // TODO
    m_buckets[Spatial::Bucket::RB_SCREEN].enable_culling = false;
    m_buckets[Spatial::Bucket::RB_DEBUG].enable_culling = false;

    // TODO: re-introduce frustum culling
    m_octree_callback_id = SceneManager::GetInstance()->GetOctree()->AddCallback([this](OctreeChangeEvent evt, const Octree *oct, int node_id, const Spatial *spatial) {
        if (spatial == nullptr) {
            return;
        }

        if (evt == OCTREE_INSERT_NODE) {
            m_buckets[spatial->GetBucket()].AddItem(BucketItem(node_id, *spatial));
        } else if (evt == OCTREE_REMOVE_NODE) {
            m_buckets[spatial->GetBucket()].RemoveItem(node_id);
        } else if (evt == OCTREE_NODE_TRANSFORM_CHANGE) {
            //m_buckets[spatial->GetRenderable()->GetRenderBucket()].SetItem(node_id, BucketItem(node_id, *spatial));
        }
    });
}

Renderer::~Renderer()
{
    SceneManager::GetInstance()->GetOctree()->RemoveCallback(m_octree_callback_id);

    delete m_deferred_pipeline;
    delete m_fbo;
}

void Renderer::Render(Camera *cam)
{
    if (m_fbo == nullptr) {
        m_fbo = new Framebuffer2D(
            m_render_window.GetScaledWidth(),
            m_render_window.GetScaledHeight(),
            true, // color
            true, // depth
            true, // normals
            true, // positions
            true, // data
            true, // ao
            true, // tangents
            true  // bitangents
        );
    }

    if (!m_buckets[Spatial::Bucket::RB_BUFFER].IsEmpty()) {
        RenderBucket(cam, m_buckets[Spatial::Bucket::RB_BUFFER]); // PRE
    }

    m_deferred_pipeline->Render(this, cam, m_fbo);

    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::CULL_FACE);
    RenderBucket(cam, m_buckets[Spatial::Bucket::RB_DEBUG]);
    RenderBucket(cam, m_buckets[Spatial::Bucket::RB_SCREEN]);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::CULL_FACE);
}

void Renderer::ClearRenderables()
{
   for (int i = 0; i < Spatial::Bucket::RB_MAX; i++) {
       m_buckets[i].ClearAll();
   }
}

void Renderer::RenderBucket(Camera *cam, Bucket &bucket, Shader *override_shader, bool enable_frustum_culling)
{
    enable_frustum_culling = enable_frustum_culling && bucket.enable_culling;

    // proceed even if no shader is set if the render bucket is BUFFER.
    bool render_if_no_shader = &bucket == &GetBucket(Spatial::Bucket::RB_BUFFER);

    Shader *shader = nullptr;
    Shader *last_shader = nullptr;

    for (const BucketItem &it : bucket.GetItems()) {
        if (!it.alive) {
            continue;
        }

        if (enable_frustum_culling && it.frustum_culled) {
            continue;
        }

        soft_assert_continue(it.GetSpatial().GetRenderable() != nullptr);

        // TODO: group by same shader
        shader = (override_shader ? override_shader : it.GetSpatial().GetRenderable()->m_shader.get());

        if (!shader && !render_if_no_shader) {
            continue;
        }

        if (shader) {
            shader->ApplyMaterial(it.GetSpatial().GetMaterial());
            shader->ApplyTransforms(it.GetSpatial().GetTransform(), cam);

#if RENDERER_SHADER_GROUPING
            if (shader != last_shader) {
                if (last_shader != nullptr) {
                    last_shader->End();
                }

                last_shader = shader;

                shader->Use(); // this will call ApplyUniforms() as well
            } else {
                shader->ApplyUniforms(); // re-using shader; have to apply uniforms manually
            }
#else
            shader->Use();
#endif

            it.GetSpatial().GetRenderable()->Render(this, cam);

#if !RENDERER_SHADER_GROUPING
            shader->End();
#endif

        } else if (render_if_no_shader) {
            it.GetSpatial().GetRenderable()->Render(this, cam);
        }

        SetRendererDefaults();
    }

#if RENDERER_SHADER_GROUPING
    if (last_shader != nullptr) {
        last_shader->End();
    }
#endif
}

void Renderer::SetRendererDefaults()
{
    CoreEngine::GetInstance()->BlendFunc(CoreEngine::GLEnums::ONE, CoreEngine::GLEnums::ZERO);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::DEPTH_TEST);
    CoreEngine::GetInstance()->DepthMask(true);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::CULL_FACE);
    CoreEngine::GetInstance()->CullFace(CoreEngine::GLEnums::BACK);
    CoreEngine::GetInstance()->BindTexture(CoreEngine::GLEnums::TEXTURE_2D, 0);
    CoreEngine::GetInstance()->BindTexture(CoreEngine::GLEnums::TEXTURE_CUBE_MAP, 0);
}

void Renderer::RenderAll(Camera *cam, Framebuffer2D *fbo)
{
    if (!m_buckets[Spatial::Bucket::RB_BUFFER].IsEmpty()) {
        RenderBucket(cam, m_buckets[Spatial::Bucket::RB_BUFFER]); // PRE
    }

    if (fbo) {
        fbo->Use();
    } else {
        CoreEngine::GetInstance()->Viewport(0, 0, cam->GetWidth(), cam->GetHeight());
    }

    CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT);

    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::CULL_FACE);
    RenderBucket(cam, m_buckets[Spatial::Bucket::RB_SKY]);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::CULL_FACE);
    RenderBucket(cam, m_buckets[Spatial::Bucket::RB_OPAQUE]);
    RenderBucket(cam, m_buckets[Spatial::Bucket::RB_TRANSPARENT]);
    RenderBucket(cam, m_buckets[Spatial::Bucket::RB_PARTICLE]);

    if (fbo) {
        fbo->End();
    }
}
} // namespace hyperion
