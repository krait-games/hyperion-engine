#ifndef HYPERION_V2_SPATIAL_H
#define HYPERION_V2_SPATIAL_H

#include "material.h"

#include <rendering/backend/renderer_structs.h>

#include <math/matrix4.h>
#include <math/transform.h>

/* TMP */
#include "../../mesh.h"

#include <vector>

namespace hyperion::v2 {

using renderer::MeshInputAttributeSet;

class GraphicsPipeline;

class Spatial : public EngineComponent<STUB_CLASS(Spatial)> {
    friend class Engine;
    friend class GraphicsPipeline;
public:
    Spatial(const std::shared_ptr<Mesh> &mesh,
        const MeshInputAttributeSet &attributes,
        const Transform &transform,
        Material::ID material_id);
    Spatial(const Spatial &other) = delete;
    Spatial &operator=(const Spatial &other) = delete;
    ~Spatial();
    
    const std::shared_ptr<Mesh> &GetMesh() const { return m_mesh; }
    const MeshInputAttributeSet &GetVertexAttributes() const { return m_attributes; }

    const Transform &GetTransform() const { return m_transform; }
    inline void SetTransform(const Transform &transform) { m_transform = transform; /* TODO: mark dirty */ }

    const Material::ID &GetMaterialId() const { return m_material_id; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);

private:
    void OnAddedToPipeline(GraphicsPipeline *pipeline);
    void OnRemovedFromPipeline(GraphicsPipeline *pipeline);
    void RemoveFromPipelines();
    void RemoveFromPipeline(GraphicsPipeline *pipeline);

    std::shared_ptr<Mesh> m_mesh;
    MeshInputAttributeSet m_attributes;
    Transform m_transform;
    Material::ID m_material_id;

    /* Retains a list of pointers to pipelines that this Spatial is used by,
     * for easy removal when RemoveSpatial() is called.
     */
    std::vector<GraphicsPipeline *> m_pipelines;
};

} // namespace hyperion::v2

#endif