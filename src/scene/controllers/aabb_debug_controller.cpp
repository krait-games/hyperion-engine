#include "aabb_debug_controller.h"
#include <engine.h>
#include <builders/mesh_builder.h>

#include <util/utf8.hpp>

namespace hyperion::v2 {

AabbDebugController::AabbDebugController(Engine *engine)
    : Controller("AabbDebugController"),
      m_engine(engine)
{
}

void AabbDebugController::OnAdded()
{
    m_aabb = GetOwner()->GetWorldAabb();
    auto *scene = GetOwner()->GetScene();

    if (scene == nullptr) {
        DebugLog(
            LogType::Error,
            "Added aabb debug controller but Entity #%u was not in scene\n",
            GetOwner()->GetId().value
        );

        return;
    }

    AssertThrow(m_engine != nullptr);

    auto mesh              = MeshBuilder::Cube();
    auto vertex_attributes = mesh->GetVertexAttributes();
    auto material          = m_engine->resources.materials.Add(std::make_unique<Material>("aabb_material"));

    auto shader            = m_engine->shader_manager.GetShader(ShaderManager::Key::DEBUG_AABB).IncRef();
    const auto shader_id   = shader != nullptr ? shader->GetId() : Shader::empty_id;

    m_aabb_entity = m_engine->resources.spatials.Add(
        std::make_unique<Spatial>(
            m_engine->resources.meshes.Add(std::move(mesh)),
            std::move(shader),
            std::move(material),
            RenderableAttributeSet {
                .bucket            = Bucket::BUCKET_TRANSLUCENT,
                .shader_id         = shader_id,
                .vertex_attributes = vertex_attributes,
                .fill_mode         = FillMode::LINE,
                .cull_faces        = FaceCullMode::BACK
            }
        )
    );

    m_aabb_entity.Init();

    scene->AddSpatial(m_aabb_entity.IncRef());
}

void AabbDebugController::OnRemoved()
{
    if (m_aabb_entity != nullptr) {
        if (auto *scene = m_aabb_entity->GetScene()) {
            scene->RemoveSpatial(m_aabb_entity);
        }

        m_aabb_entity = nullptr;
    }
}

void AabbDebugController::OnUpdate(GameCounter::TickUnit delta)
{
    if (m_aabb != GetOwner()->GetWorldAabb()) {
        m_aabb = GetOwner()->GetWorldAabb();
    }

    if (m_aabb_entity == nullptr) {
        return;
    }

    m_aabb_entity->SetTransform(Transform(
        GetOwner()->GetTranslation() + Vector3(0, m_aabb.GetDimensions().y * 0.5f, 0),
        m_aabb.GetDimensions() * 0.5f,
        Quaternion::Identity()
    ));
}

} // namespace hyperion::v2