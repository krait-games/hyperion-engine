#include "Entity.hpp"
#include <math/BoundingSphere.hpp>
#include <rendering/Renderer.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

Entity::Entity(
    Ref<Mesh> &&mesh,
    Ref<Shader> &&shader,
    Ref<Material> &&material,
    const ComponentInitInfo &init_info
) : Entity(
      std::move(mesh),
      std::move(shader),
      std::move(material),
      {},
      init_info
    )
{
    // RebuildRenderableAttributes();
}

Entity::Entity(
    Ref<Mesh> &&mesh,
    Ref<Shader> &&shader,
    Ref<Material> &&material,
    const RenderableAttributeSet &renderable_attributes,
    const ComponentInitInfo &init_info
) : EngineComponentBase(init_info),
    HasDrawProxy(),
    m_mesh(std::move(mesh)),
    m_shader(std::move(shader)),
    m_material(std::move(material)),
    m_node(nullptr),
    m_scene(nullptr),
    m_renderable_attributes(renderable_attributes),
    m_octree(nullptr),
    m_needs_octree_update(false),
    m_shader_data_state(ShaderDataState::DIRTY)
{
    if (m_mesh) {
        m_local_aabb = m_mesh->CalculateAabb();
        m_world_aabb = m_local_aabb * m_transform;
    }
}

Entity::~Entity()
{
    Teardown();
}

void Entity::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    m_draw_proxy.entity_id = m_id;

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SPATIALS, [this](...) {
        auto *engine = GetEngine();

        if (m_material) {
            m_material.Init();
        }

        if (m_skeleton) {
            m_skeleton.Init();
        }

        if (m_mesh) {
            m_mesh.Init();
        }

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SPATIALS, [this](...) {
            auto *engine = GetEngine();

            SetReady(false);

            m_material.Reset();

            engine->SafeReleaseRenderResource<Skeleton>(std::move(m_skeleton));
            engine->SafeReleaseRenderResource<Mesh>(std::move(m_mesh));
            engine->SafeReleaseRenderResource<Shader>(std::move(m_shader));
        }));
    }));
}

void Entity::Update(Engine *engine, GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    if (m_skeleton != nullptr && m_skeleton->IsReady()) {
        m_skeleton->EnqueueRenderUpdates();
    }

    if (m_material != nullptr && m_material->IsReady()) {
        m_material->Update(engine);
    }

    UpdateControllers(engine, delta);

    if (m_needs_octree_update) {
        UpdateOctree();
    }

    if (m_shader_data_state.IsDirty()) {
        EnqueueRenderUpdates();
    }
}

void Entity::UpdateControllers(Engine *engine, GameCounter::TickUnit delta)
{
    for (auto &it : m_controllers) {
        if (!it.second->ReceivesUpdate()) {
            continue;
        }

        it.second->OnUpdate(delta);
    }
}

void Entity::EnqueueRenderUpdates()
{
    AssertReady();

    const Skeleton::ID skeleton_id = m_skeleton != nullptr
        ? m_skeleton->GetId()
        : Skeleton::empty_id;

    const Material::ID material_id = m_material != nullptr
        ? m_material->GetId()
        : Material::empty_id;

    const Mesh::ID mesh_id = m_mesh != nullptr
        ? m_mesh->GetId()
        : Mesh::empty_id;

    const Scene::ID scene_id = m_scene != nullptr
        ? m_scene->GetId()
        : Scene::empty_id;

    const EntityDrawProxy draw_proxy {
        .mesh         = m_mesh.ptr,
        .material     = m_material.ptr,
        .entity_id    = m_id,
        .scene_id     = scene_id,
        .mesh_id      = mesh_id,
        .material_id  = material_id,
        .skeleton_id  = skeleton_id,
        .bounding_box = m_world_aabb
    };

    GetEngine()->render_scheduler.Enqueue([this, transform = m_transform, draw_proxy](...) {
        // update m_draw_proxy on render thread.
        m_draw_proxy = draw_proxy;

        GetEngine()->shader_globals->objects.Set(
            m_id.value - 1,
            ObjectShaderData {
                .model_matrix   = transform.GetMatrix(),

                .local_aabb_max = Vector4(m_local_aabb.max, 1.0f),
                .local_aabb_min = Vector4(m_local_aabb.min, 1.0f),
                .world_aabb_max = Vector4(m_world_aabb.max, 1.0f),
                .world_aabb_min = Vector4(m_world_aabb.min, 1.0f),

                .entity_id      = draw_proxy.entity_id.value,
                .scene_id       = draw_proxy.scene_id.value,
                .mesh_id        = draw_proxy.mesh_id.value,
                .material_id    = draw_proxy.material_id.value,
                .skeleton_id    = draw_proxy.skeleton_id.value
            }
        );

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}

void Entity::UpdateOctree()
{
    AssertThrow(IsInitCalled());

    if (Octree *octree = m_octree.load()) {
        const auto update_result = octree->Update(GetEngine(), this);

        if (!update_result) {
            DebugLog(
                LogType::Warn,
                "Could not update Entity #%u in octree: %s\n",
                m_id.value,
                update_result.message
            );
        }
    }

    m_needs_octree_update = false;
}

void Entity::SetMesh(Ref<Mesh> &&mesh)
{
    if (m_mesh == mesh) {
        return;
    }

    if (m_mesh != nullptr) {
        GetEngine()->SafeReleaseRenderResource<Mesh>(std::move(m_mesh));
    }

    m_mesh = std::move(mesh);

    if (m_mesh != nullptr && IsReady()) {
        m_mesh.Init();
    }

    m_shader_data_state |= ShaderDataState::DIRTY;
    m_primary_renderer_instance.changed = true;
}

void Entity::SetSkeleton(Ref<Skeleton> &&skeleton)
{
    if (m_skeleton == skeleton) {
        return;
    }

    if (m_skeleton != nullptr) {
        GetEngine()->SafeReleaseRenderResource<Skeleton>(std::move(m_skeleton));
    }

    m_skeleton = std::move(skeleton);

    if (m_skeleton != nullptr && IsReady()) {
        m_skeleton.Init();
    }

    m_shader_data_state |= ShaderDataState::DIRTY;
    m_primary_renderer_instance.changed = true;
}

void Entity::SetShader(Ref<Shader> &&shader)
{
    if (m_shader == shader) {
        return;
    }

    if (m_shader != nullptr) {
        GetEngine()->SafeReleaseRenderResource<Shader>(std::move(m_shader));
    }

    m_shader = std::move(shader);
    
    RenderableAttributeSet new_renderable_attributes(m_renderable_attributes);
    new_renderable_attributes.shader_id = m_shader->GetId();

    SetRenderableAttributes(new_renderable_attributes);

    if (m_shader != nullptr && IsReady()) {
        m_shader.Init();
    }

    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Entity::SetMaterial(Ref<Material> &&material)
{
    if (m_material == material) {
        return;
    }

    //if (m_material != nullptr) {
    //    GetEngine()->SafeReleaseRenderResource(std::move(m_material));
    //}

    m_material = std::move(material);

    if (m_material != nullptr && IsReady()) {
        m_material.Init();
    }

    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Entity::SetParent(Node *node)
{
    if (m_node != nullptr) {
        for (auto &controller : m_controllers) {
            AssertThrow(controller.second != nullptr);

            controller.second->OnRemovedFromNode(m_node);
        }
    }

    m_node = node;

    if (m_node != nullptr) {
        for (auto &controller : m_controllers) {
            controller.second->OnAddedToNode(m_node);
        }
    }
}

void Entity::SetScene(Scene *scene)
{
    m_scene = scene;

    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Entity::SetRenderableAttributes(const RenderableAttributeSet &renderable_attributes)
{
    if (m_renderable_attributes == renderable_attributes) {
        return;
    }

    m_renderable_attributes    = renderable_attributes;
    m_primary_renderer_instance.changed = true;
}

void Entity::RebuildRenderableAttributes()
{
    RenderableAttributeSet new_renderable_attributes(m_renderable_attributes);

    if (m_mesh != nullptr) {
        new_renderable_attributes.vertex_attributes = m_mesh->GetVertexAttributes();
        new_renderable_attributes.topology          = m_mesh->GetTopology();
    } else {
        new_renderable_attributes.vertex_attributes = {};
        new_renderable_attributes.topology          = Topology::TRIANGLES;
    }

    if (m_skeleton != nullptr) {
        new_renderable_attributes.vertex_attributes = new_renderable_attributes.vertex_attributes | renderer::skeleton_vertex_attributes;
    }

    new_renderable_attributes.shader_id = m_shader != nullptr
        ? m_shader->GetId()
        : Shader::empty_id;
    

    // if (m_material != nullptr) {
    //     new_renderable_attributes.cull_faces        = face_cull_mode;
    //     new_renderable_attributes.depth_write       = depth_write;
    //     new_renderable_attributes.depth_test        = depth_test;
    // }

    SetRenderableAttributes(new_renderable_attributes);
}

void Entity::SetMeshAttributes(
    VertexAttributeSet vertex_attributes,
    FaceCullMode face_cull_mode,
    bool depth_write,
    bool depth_test
)
{
    RenderableAttributeSet new_renderable_attributes(m_renderable_attributes);
    new_renderable_attributes.vertex_attributes = vertex_attributes;
    new_renderable_attributes.cull_faces        = face_cull_mode;
    new_renderable_attributes.depth_write       = depth_write;
    new_renderable_attributes.depth_test        = depth_test;

    SetRenderableAttributes(new_renderable_attributes);
}

void Entity::SetMeshAttributes(
    FaceCullMode face_cull_mode,
    bool depth_write,
    bool depth_test
)
{
    SetMeshAttributes(
        m_renderable_attributes.vertex_attributes,
        face_cull_mode,
        depth_write,
        depth_test
    );
}

void Entity::SetStencilAttributes(const StencilState &stencil_state)
{
    RenderableAttributeSet new_renderable_attributes(m_renderable_attributes);
    new_renderable_attributes.stencil_state = stencil_state;

    SetRenderableAttributes(new_renderable_attributes);
}

void Entity::SetBucket(Bucket bucket)
{
    RenderableAttributeSet new_renderable_attributes(m_renderable_attributes);
    new_renderable_attributes.bucket = bucket;

    SetRenderableAttributes(new_renderable_attributes);
}

void Entity::SetTranslation(const Vector3 &translation)
{
    if (m_node != nullptr) {
        // indirectly calls SetTransform() on this
        m_node->SetWorldTranslation(translation);
    } else {
        Transform new_transform(m_transform);
        new_transform.SetTranslation(translation);

        SetTransform(new_transform);
    }
}

void Entity::SetScale(const Vector3 &scale)
{
    if (m_node != nullptr) {
        // indirectly calls SetTransform() on this
        m_node->SetWorldScale(scale);
    } else {
        Transform new_transform(m_transform);
        new_transform.SetScale(scale);

        SetTransform(new_transform);
    }
}

void Entity::SetRotation(const Quaternion &rotation)
{
    if (m_node != nullptr) {
        // indirectly calls SetTransform() on this
        m_node->SetWorldRotation(rotation);
    } else {
        Transform new_transform(m_transform);
        new_transform.SetRotation(rotation);

        SetTransform(new_transform);
    }
}

void Entity::SetTransform(const Transform &transform)
{
    if (m_transform == transform) {
        return;
    }

    m_transform = transform;
    m_shader_data_state |= ShaderDataState::DIRTY;

    m_world_aabb = m_local_aabb * transform;

    for (auto &it : m_controllers) {
        it.second->OnTransformUpdate(m_transform);
    }

    if (IsInitCalled()) {
        UpdateOctree();
    } else {
        m_needs_octree_update = true;
    }
}

void Entity::OnAddedToPipeline(RendererInstance *pipeline)
{
    m_renderer_instances.Insert(pipeline);
}

void Entity::OnRemovedFromPipeline(RendererInstance *pipeline)
{
    if (pipeline == m_primary_renderer_instance.renderer_instance) {
        m_primary_renderer_instance = {
            .renderer_instance = nullptr,
            .changed  = true
        };
    }

    m_renderer_instances.Erase(pipeline);
}

void Entity::OnAddedToOctree(Octree *octree)
{
    AssertThrow(m_octree == nullptr);
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Entity #%lu added to octree\n", m_id.value);
#endif

    m_octree = octree;

    if (IsInitCalled()) {
        UpdateOctree();
    } else {
        m_needs_octree_update = true;
    }

    // m_shader_data_state |= ShaderDataState::DIRTY;
}

void Entity::OnRemovedFromOctree(Octree *octree)
{
    AssertThrow(octree == m_octree);
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Entity #%lu removed from octree\n", m_id.value);
#endif

    m_octree = nullptr;
    // m_shader_data_state |= ShaderDataState::DIRTY;
}

void Entity::OnMovedToOctant(Octree *octree)
{
    AssertThrow(m_octree != nullptr);
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Entity #%lu moved to new octant\n", m_id.value);
#endif

    m_octree = octree;

    if (IsInitCalled()) {
        UpdateOctree();
    } else {
        m_needs_octree_update = true;
    }

    // m_shader_data_state |= ShaderDataState::DIRTY;
}


void Entity::AddToOctree(Engine *engine, Octree &octree)
{
    AssertThrow(m_octree == nullptr);

    if (!octree.Insert(engine, this)) {
        DebugLog(LogType::Warn, "Entity #%lu could not be added to octree\n", m_id.value);
    }
}

void Entity::RemoveFromOctree(Engine *engine)
{
    DebugLog(
        LogType::Debug,
        "Remove entity #%u from octree\n",
        GetId().value
    );

    m_octree.load()->OnEntityRemoved(engine, this);
}

bool Entity::IsReady() const
{
    /*if (!Base::IsReady()) {
        return false;
    }
    
    if (m_skeleton != nullptr && !m_skeleton->IsReady()) {
        return false;
    }
    
    if (m_shader != nullptr && !m_shader->IsReady()) {
        return false;
    }
    
    if (m_mesh != nullptr && !m_mesh->IsReady()) {
        return false;
    }

    if (m_material != nullptr && !m_material->IsReady()) {
        return false;
    }
    
    return true;*/

    return Base::IsReady();
}

} // namespace hyperion::v2
