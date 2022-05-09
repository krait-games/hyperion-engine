#ifndef RENDERER_ACCELERATION_STRUCTURE_H
#define RENDERER_ACCELERATION_STRUCTURE_H

#include "../renderer_result.h"
#include "../renderer_buffer.h"
#include "../renderer_structs.h"

#include <vector>
#include <memory>

#include "renderer_acceleration_structure.h"

namespace hyperion {
namespace renderer {

class Instance;
class AccelerationStructure;

enum class AccelerationStructureType {
    BOTTOM_LEVEL,
    TOP_LEVEL
};

enum AccelerationStructureFlags {
    ACCELERATION_STRUCTURE_FLAGS_NONE = 0,
    ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING = 1
};

class AccelerationGeometry {
    friend class AccelerationStructure;
    friend class TopLevelAccelerationStructure;
    friend class BottomLevelAccelerationStructure;

public:
    AccelerationGeometry(
        std::vector<PackedVertex> &&packed_vertices,
        std::vector<PackedIndex>  &&packed_indices
    );

    AccelerationGeometry(const AccelerationGeometry &other) = delete;
    AccelerationGeometry &operator=(const AccelerationGeometry &other) = delete;
    ~AccelerationGeometry();

    inline const std::vector<PackedVertex> &GetPackedVertices() const { return m_packed_vertices; }
    inline const std::vector<PackedIndex> &GetPackedIndices() const   { return m_packed_indices; }

    inline PackedVertexStorageBuffer *GetPackedVertexStorageBuffer() const { return m_packed_vertex_buffer.get(); }
    inline PackedIndexStorageBuffer  *GetPackedIndexStorageBuffer() const  { return m_packed_index_buffer.get(); }

    Result Create(Instance *instance);
    /* Remove from the parent acceleration structure */
    Result Destroy(Instance *instance);

private:
    std::vector<PackedVertex> m_packed_vertices;
    std::vector<PackedIndex>  m_packed_indices;

    std::unique_ptr<PackedVertexStorageBuffer> m_packed_vertex_buffer;
    std::unique_ptr<PackedIndexStorageBuffer>  m_packed_index_buffer;

    VkAccelerationStructureGeometryKHR m_geometry;
};

class AccelerationStructure {
public:
    AccelerationStructure();
    AccelerationStructure(const AccelerationStructure &other) = delete;
    AccelerationStructure &operator=(const AccelerationStructure &other) = delete;
    ~AccelerationStructure();

    inline AccelerationStructureBuffer *GetBuffer() const                     { return m_buffer.get(); }
    inline AccelerationStructureInstancesBuffer *GetInstancesBuffer() const   { return m_instances_buffer.get(); }

    inline VkAccelerationStructureKHR &GetAccelerationStructure()             { return m_acceleration_structure; }
    inline const VkAccelerationStructureKHR &GetAccelerationStructure() const { return m_acceleration_structure; }

    inline uint64_t GetDeviceAddress() const                                  { return m_device_address; }

    inline AccelerationStructureFlags GetFlags() const                        { return m_flags; }
    inline void SetFlags(AccelerationStructureFlags flags)                    { m_flags = flags; }

    inline const std::vector<std::unique_ptr<AccelerationGeometry>> &GetGeometries() const
        { return m_geometries; }

    inline void AddGeometry(std::unique_ptr<AccelerationGeometry> &&geometry)
        { m_geometries.push_back(std::move(geometry)); SetNeedsUpdateFlag(); }

    inline const Matrix4 &GetTransform() const         { return m_transform; }
    inline void SetTransform(const Matrix4 &transform) { m_transform = transform; SetNeedsUpdateFlag(); }

    /*! \brief Remove the geometry from the internal list of Nodes and set a flag that the
     * structure needs to be rebuilt. Will not automatically rebuild.
     */
    void RemoveGeometry(AccelerationGeometry *geometry);

    Result Destroy(Instance *instance);

protected:
    static VkAccelerationStructureTypeKHR ToVkAccelerationStructureType(AccelerationStructureType);

    inline void SetFlag(AccelerationStructureFlags flag)   { m_flags = AccelerationStructureFlags(m_flags | flag); }
    inline void ClearFlag(AccelerationStructureFlags flag) { m_flags = AccelerationStructureFlags(m_flags & ~flag); }
    inline void SetNeedsUpdateFlag()                       { SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING); }

    Result CreateAccelerationStructure(Instance *instance,
        AccelerationStructureType type,
        std::vector<VkAccelerationStructureGeometryKHR> &&geometries,
        std::vector<uint32_t> &&primitive_counts,
        bool update = false);
    
    std::unique_ptr<AccelerationStructureBuffer>          m_buffer;
    std::unique_ptr<AccelerationStructureInstancesBuffer> m_instances_buffer;
    std::vector<std::unique_ptr<AccelerationGeometry>>    m_geometries;
    Matrix4                                               m_transform;
    VkAccelerationStructureKHR                            m_acceleration_structure;
    uint64_t                                              m_device_address;
    AccelerationStructureFlags                            m_flags;
};

class BottomLevelAccelerationStructure : public AccelerationStructure {
public:
    BottomLevelAccelerationStructure();
    BottomLevelAccelerationStructure(const BottomLevelAccelerationStructure &other) = delete;
    BottomLevelAccelerationStructure &operator=(const BottomLevelAccelerationStructure &other) = delete;
    ~BottomLevelAccelerationStructure();

    inline AccelerationStructureType GetType() const { return AccelerationStructureType::BOTTOM_LEVEL; }
    
    Result Create(Instance *instance);

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    Result UpdateStructure(Instance *instance);

private:
    Result Rebuild(Instance *instance);
};

class TopLevelAccelerationStructure : public AccelerationStructure {
public:
    TopLevelAccelerationStructure();
    TopLevelAccelerationStructure(const TopLevelAccelerationStructure &other) = delete;
    TopLevelAccelerationStructure &operator=(const TopLevelAccelerationStructure &other) = delete;
    ~TopLevelAccelerationStructure();

    inline AccelerationStructureType GetType() const                          { return AccelerationStructureType::TOP_LEVEL; }

    inline StorageBuffer *GetMeshDescriptionsBuffer() const                   { return m_mesh_descriptions_buffer.get(); }
    
    Result Create(Instance *instance,
        std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> &&blas);

    Result Destroy(Instance *instance);

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    Result UpdateStructure(Instance *instance);

private:
    Result Rebuild(Instance *instance);

    Result CreateInstancesBuffer(Instance *instance);
    
    Result CreateMeshDescriptionsBuffer(Instance *instance);
    Result RebuildMeshDescriptionsBuffer(Instance *instance);

    std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> m_blas;
    std::unique_ptr<StorageBuffer>                                 m_mesh_descriptions_buffer;
};

} // namespace renderer
} // namespace hyperion

#endif
