#include "gi_manager.h"
#include "../renderer.h"
#include "../shader_manager.h"
#include "../shaders/gi/gi_voxel_shader.h"
#include "../shaders/gi/gi_voxel_clear_shader.h"

namespace hyperion {
const int GIManager::voxel_map_size = 64;

GIManager *GIManager::instance = nullptr;

GIManager *GIManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new GIManager();
    }

    return instance;
}

GIManager::GIManager()
{
    ShaderManager::GetInstance()->SetBaseShaderProperties(
        ShaderProperties()
            .Define("VCT_MAP_SIZE", voxel_map_size)
            .Define("VCT_NUM_MAPS", 6)
            .Define("VCT_GEOMETRY_SHADER", true)
    );
}
} // namespace apex