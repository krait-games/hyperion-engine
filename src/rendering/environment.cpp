#include "environment.h"
#include <engine.h>

#include <rendering/backend/renderer_frame.h>

namespace hyperion::v2 {

Environment::Environment()
    : EngineComponentBase()
{
}

Environment::~Environment()
{
    Teardown();
}

void Environment::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }
    
    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ENVIRONMENTS, [this](Engine *engine) {
        /*for (auto &texture : m_environment_textures) {
            if (texture != nullptr) {
                texture.Init();
            }
        }*/

        for (auto &light : m_lights) {
            if (light != nullptr) {
                light.Init();
            }
        }

        for (auto &shadow_renderer : m_shadow_renderers) {
            AssertThrow(shadow_renderer != nullptr);

            shadow_renderer->Init(engine);
        }

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ENVIRONMENTS, [this](Engine *engine) {
            m_shadow_renderers.clear();

            HYP_FLUSH_RENDER_QUEUE(engine);

            SetReady(false);
        }), engine);
    }));
}

void Environment::AddLight(Ref<Light> &&light)
{
    if (light != nullptr && IsReady()) {
        light.Init();
    }

    m_lights.push_back(std::move(light));
}


void Environment::AddShadowRenderer(Engine *engine, std::unique_ptr<ShadowRenderer> &&shadow_renderer)
{
    AssertThrow(shadow_renderer != nullptr);

    if (IsReady()) {
        shadow_renderer->Init(engine);
    }

    m_shadow_renderers.push_back(std::move(shadow_renderer));
}

void Environment::RemoveShadowRenderer(Engine *engine, size_t index)
{
    m_shadow_renderers.erase(m_shadow_renderers.begin() + index);
}

void Environment::RenderShadows(Engine *engine, Frame *frame)
{
    AssertReady();

    /* TODO: have observer on octrees, only update shadow renderers
     * who's lights have had objects move within their octant
     */

    for (const auto &shadow_renderer : m_shadow_renderers) {
        shadow_renderer->Render(
            engine,
            frame->GetCommandBuffer(),
            frame->GetFrameIndex()
        );
    }
}

} // namespace hyperion::v2