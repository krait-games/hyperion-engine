#include "terrain_paging_controller.h"
#include <rendering/texture.h>
#include <builders/mesh_builder.h>
#include <engine.h>

namespace hyperion::v2 {

TerrainPagingController::TerrainPagingController(
    Seed seed,
    Extent3D patch_size,
    const Vector3 &scale
) : PagingController("TerrainPagingController", patch_size, scale),
    m_seed(seed),
    m_add_generated_chunks_timer(0.0f)
{
}

void TerrainPagingController::OnAdded()
{
    m_material = GetEngine()->resources.materials.Add(std::make_unique<Material>(
        "terrain_material"
    ));

    m_material->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(0.2f, 0.99f, 0.5f, 1.0f));
    m_material->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.8f);
    m_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, GetEngine()->resources.textures.Add(GetEngine()->assets.Load<Texture>("textures/forest-floor-unity/forest_floor_albedo.png")));
    m_material->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, GetEngine()->resources.textures.Add(GetEngine()->assets.Load<Texture>("textures/forest-floor-unity/forest_floor_Normal-ogl.png")));
    m_material->SetTexture(Material::MATERIAL_TEXTURE_AO_MAP, GetEngine()->resources.textures.Add(GetEngine()->assets.Load<Texture>("textures/forest-floor-unity/forest_floor-ao.png")));
    m_material->SetTexture(Material::MATERIAL_TEXTURE_PARALLAX_MAP, GetEngine()->resources.textures.Add(GetEngine()->assets.Load<Texture>("textures/forest-floor-unity/forest_floor_Height.png")));

    m_material.Init();

    PagingController::OnAdded();
}

void TerrainPagingController::OnRemoved()
{
    PagingController::OnRemoved();
}

void TerrainPagingController::OnUpdate(GameCounter::TickUnit delta)
{
    if (m_terrain_generation_flag.load()) {
        AddEnqueuedChunks();
    }

    PagingController::OnUpdate(delta);
}

void TerrainPagingController::OnPatchAdded(Patch *patch)
{
    DebugLog(LogType::Info, "Terrain patch added at [%f, %f], enqueuing terrain generation\n", patch->info.coord.x, patch->info.coord.y);
    
    GetEngine()->terrain_thread.ScheduleTask([this, patch_info = patch->info]() {
        TerrainMeshBuilder builder(patch_info);
        builder.GenerateHeights(m_seed);

        auto mesh = builder.BuildMesh();

        std::lock_guard guard(m_terrain_generation_mutex);

        m_terrain_mesh_queue.push(TerrainGenerationResult {
            .patch_info = patch_info,
            .mesh       = std::move(mesh)
        });

        m_terrain_generation_flag.store(true);
    });
}

void TerrainPagingController::OnPatchRemoved(Patch *patch)
{
    DebugLog(LogType::Info, "Terrain patch removed %f, %f\n", patch->info.coord.x, patch->info.coord.y);

    if (patch->spatial != nullptr) {
        if (auto *scene = GetOwner()->GetScene()) {
            DebugLog(LogType::Debug, "Remove terrain spatial with id #%u\n", patch->spatial->GetId().value);

            scene->RemoveSpatial(patch->spatial);
        }

        patch->spatial = nullptr;
    }
}

void TerrainPagingController::AddEnqueuedChunks()
{
    std::lock_guard guard(m_terrain_generation_mutex);

    size_t num_chunks_added = 0;

    while (!m_terrain_mesh_queue.empty()) {
        auto &terrain_generation_result = m_terrain_mesh_queue.front();
        const auto &patch_info          = terrain_generation_result.patch_info;

        auto mesh = GetEngine()->resources.meshes.Add(std::move(terrain_generation_result.mesh));
        auto vertex_attributes = mesh->GetVertexAttributes();

        auto &shader = GetEngine()->shader_manager.GetShader(ShaderManager::Key::BASIC_FORWARD);
        
        auto spatial = std::make_unique<Spatial>(
            std::move(mesh),
            shader.IncRef(),
            m_material.IncRef(),
            RenderableAttributeSet {
                .bucket            = Bucket::BUCKET_OPAQUE,
                .shader_id         = shader->GetId(),
                .vertex_attributes = vertex_attributes
            }
        );

        spatial->SetTranslation({
            (patch_info.coord.x - 0.5f) * (patch_info.extent.ToVector3().Avg() - 1) * m_scale.x,
            GetOwner()->GetTranslation().y,
            (patch_info.coord.y - 0.5f) * (patch_info.extent.ToVector3().Avg() - 1) * m_scale.z
        });

        if (auto *patch = GetPatch(patch_info.coord)) {
            if (patch->spatial == nullptr) {
                patch->spatial = GetEngine()->resources.spatials.Add(std::move(spatial));

                ++num_chunks_added;

                if (auto *scene = GetOwner()->GetScene()) {
                    DebugLog(LogType::Debug, "Add terrain spatial with id #%u\n", patch->spatial->GetId().value);
                    scene->AddSpatial(patch->spatial.IncRef());
                }
            } else {
                DebugLog(
                    LogType::Warn,
                    "Patch at [%f, %f] already had an entity assigned!\n",
                    patch_info.coord.x, patch_info.coord.y
                );
            }
        } else {
            DebugLog(
                LogType::Warn,
                "Patch at [%f, %f] does not exist after generation completeted!\n",
                patch_info.coord.x, patch_info.coord.y
            );
        }

        m_terrain_mesh_queue.pop();
    }

    DebugLog(LogType::Debug, "Added %llu chunks\n", num_chunks_added);

    m_terrain_generation_flag.store(false);
}

} // namespace hyperion::v2