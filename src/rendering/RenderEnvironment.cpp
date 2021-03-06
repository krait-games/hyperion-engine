#include "RenderEnvironment.hpp"
#include <Engine.hpp>

#include <rendering/backend/RendererFrame.hpp>

namespace hyperion::v2 {

RenderEnvironment::RenderEnvironment(Scene *scene)
    : EngineComponentBase(),
      m_scene(scene),
      m_global_timer(0.0f),
      m_current_enabled_render_components_mask(0),
      m_next_enabled_render_components_mask(0)
{
}

RenderEnvironment::~RenderEnvironment()
{
    Teardown();
}

void RenderEnvironment::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }
    
    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ENVIRONMENTS, [this](...) {
        auto *engine = GetEngine();

        const auto update_marker_value = m_update_marker.load();

        { // lights
            if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_LIGHTS) {
                std::lock_guard guard(m_light_update_mutex);

                while (m_lights_pending_addition.Any()) {
                    auto &front = m_lights_pending_addition.Front();

                    if (front != nullptr) {
                        const auto id = front->GetId();

                        m_lights.Insert(id, std::move(front));
                    }

                    m_lights_pending_addition.Pop();
                }

                while (m_lights_pending_removal.Any()) {
                    auto &front = m_lights_pending_removal.Front();

                    if (front != nullptr) {
                        const auto id = front->GetId();

                        m_lights.Erase(id);
                    }

                    m_lights_pending_removal.Pop();
                }

                m_update_marker &= ~RENDER_ENVIRONMENT_UPDATES_LIGHTS;
            }

            for (auto &it: m_lights) {
                auto &light = it.second;

                if (light != nullptr) {
                    light.Init();
                }
            }
        }

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ENVIRONMENTS, [this](...) {
            auto *engine = GetEngine();

            m_lights.Clear();

            const auto update_marker_value = m_update_marker.load();

            if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_LIGHTS) {
                std::lock_guard guard(m_light_update_mutex);

                m_lights_pending_addition.Clear();
                m_lights_pending_removal.Clear();
            }

            m_render_components.Clear();

            if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS) {
                std::lock_guard guard(m_render_component_mutex);

                m_render_components_pending_addition.Clear();
                m_render_components_pending_removal.Clear();
            }

            if (update_marker_value) {
                m_update_marker.store(RENDER_ENVIRONMENT_UPDATES_NONE);
            }

            HYP_FLUSH_RENDER_QUEUE(engine);

            SetReady(false);
        }));
    }));
}

void RenderEnvironment::AddLight(Ref<Light> &&light)
{
    if (light != nullptr && IsReady()) {
        light.Init();
    }

    std::lock_guard guard(m_light_update_mutex);

    m_lights_pending_addition.Push(std::move(light));
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_LIGHTS;
}

void RenderEnvironment::RemoveLight(Ref<Light> &&light)
{
    std::lock_guard guard(m_light_update_mutex);

    m_lights_pending_removal.Push(std::move(light));
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_LIGHTS;
}

void RenderEnvironment::Update(Engine *engine, GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    m_global_timer += delta;

    for (auto &it : m_lights) {
        it.second->Update(engine, delta);
    }

    HYP_USED AtomicWaiter waiter(m_updating_render_components);

    for (const auto &component : m_render_components) {
        component.second->ComponentUpdate(engine, delta);
    }
}

void RenderEnvironment::OnEntityAdded(Ref<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    m_entities_pending_addition.Push(entity.IncRef());
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_ENTITIES;
}

void RenderEnvironment::OnEntityRemoved(Ref<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    m_entities_pending_removal.Push(entity.IncRef());
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_ENTITIES;
}

// only called when meaningful attributes have changed
void RenderEnvironment::OnEntityRenderableAttributesChanged(Ref<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    m_entity_renderable_attribute_updates.Push(entity.IncRef());
    
    m_update_marker |= RENDER_ENVIRONMENT_UPDATES_ENTITIES;
}

void RenderEnvironment::RenderComponents(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    m_current_enabled_render_components_mask = m_next_enabled_render_components_mask;

    const auto update_marker_value = m_update_marker.load();

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_LIGHTS) {
        std::lock_guard guard(m_light_update_mutex);

        while (m_lights_pending_addition.Any()) {
            auto &front = m_lights_pending_addition.Front();

            if (front != nullptr) {
                const auto id = front->GetId();

                GetEngine()->render_state.BindLight(id);

                m_lights.Insert(id, std::move(front));
            }

            m_lights_pending_addition.Pop();
        }

        while (m_lights_pending_removal.Any()) {
            const auto &front = m_lights_pending_removal.Front();

            if (front != nullptr) {
                const auto id = front->GetId();

                // TODO: scene switch wouldnt unbind light
                GetEngine()->render_state.UnbindLight(id);

                m_lights.Erase(id);
            }

            m_lights_pending_removal.Pop();
        }
    }

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_ENTITIES) {
        // perform updates to all RenderComponents in the render thread
        std::lock_guard guard(m_entity_update_mutex);

        while (m_entities_pending_addition.Any()) {
            for (auto &it : m_render_components) {
                it.second->OnEntityAdded(m_entities_pending_addition.Front());
            }

            m_entities_pending_addition.Pop();
        }

        while (m_entity_renderable_attribute_updates.Any()) {
            for (auto &it : m_render_components) {
                it.second->OnEntityRenderableAttributesChanged(m_entity_renderable_attribute_updates.Front());
            }

            m_entity_renderable_attribute_updates.Pop();
        }

        while (m_entities_pending_removal.Any()) {
            for (auto &it : m_render_components) {
                it.second->OnEntityRemoved(m_entities_pending_removal.Front());
            }

            m_entities_pending_removal.Pop();
        }
    }

    for (const auto &component : m_render_components) {
        m_next_enabled_render_components_mask |= 1u << static_cast<UInt32>(component.second->GetName());
        component.second->ComponentRender(engine, frame);
    }

    // add RenderComponents deferred so that the render scheduler can complete
    // initialization tasks before we call ComponentRender() on the newly added RenderComponents

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS) {
        AtomicLocker locker(m_updating_render_components);

        std::lock_guard guard(m_render_component_mutex);

        for (auto &it : m_render_components_pending_addition) {
            AssertThrow(it.second != nullptr);

            it.second->SetComponentIndex(0); // just using zero for now, when multiple of same components are supported, we will extend this
            it.second->ComponentInit(engine);

            m_render_components.Set(it.first, std::move(it.second));
        }

        m_render_components_pending_addition.Clear();

        for (const auto &it : m_render_components_pending_removal) {
            m_next_enabled_render_components_mask &= ~(1u << static_cast<UInt32>(it.second));
            m_render_components.Remove(it.first);
        }

        m_render_components_pending_removal.Clear();
    }

    if (update_marker_value != RENDER_ENVIRONMENT_UPDATES_NONE) {
        m_update_marker.store(RENDER_ENVIRONMENT_UPDATES_NONE);
    }
}

} // namespace hyperion::v2
