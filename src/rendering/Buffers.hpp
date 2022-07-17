#ifndef HYPERION_V2_BUFFERS_H
#define HYPERION_V2_BUFFERS_H

#include <util/Defines.hpp>

#include <core/lib/HeapArray.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Range.hpp>

#include <Types.hpp>

#include <memory>
#include <atomic>
#include <climits>

#define HYP_BUFFERS_USE_SPINLOCK 0

namespace hyperion::renderer {

class Device;

} // namespace hyperion::renderer

namespace hyperion::v2 {

using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::StorageBuffer;
using renderer::PerFrameData;
using renderer::Device;

struct ShaderDataState {
    enum State {
        CLEAN,
        DIRTY
    };

    ShaderDataState(State value = CLEAN) : state(value) {}
    ShaderDataState(const ShaderDataState &other) = default;
    ShaderDataState &operator=(const ShaderDataState &other) = default;

    ShaderDataState &operator=(State value)
    {
        state = value;

        return *this;
    }

    operator bool() const { return state == CLEAN; }

    bool operator==(const ShaderDataState &other) const { return state == other.state; }
    bool operator!=(const ShaderDataState &other) const { return state != other.state; }

    ShaderDataState &operator|=(State value)
    {
        state |= UInt32(value);

        return *this;
    }

    ShaderDataState &operator&=(State value)
    {
        state &= UInt32(value);

        return *this;
    }

    bool IsClean() const { return state == CLEAN; }
    bool IsDirty() const { return state == DIRTY; }

private:
    UInt32 state;
};

struct alignas(256) CubemapUniforms {
    Matrix4 projection_matrices[6];
    Matrix4 view_matrices[6];
};

struct alignas(256) SkeletonShaderData {
    static constexpr size_t max_bones = 128;

    Matrix4 bones[max_bones];
};

struct alignas(256) ObjectShaderData {
    Matrix4 model_matrix;
    UInt32 has_skinning;
    UInt32 entity_id;
    UInt32 mesh_id;
    UInt32 material_id;

    Vector4 local_aabb_max;
    Vector4 local_aabb_min;
    Vector4 world_aabb_max;
    Vector4 world_aabb_min;
};

static_assert(sizeof(ObjectShaderData) == 256);

struct MaterialShaderData {
    static constexpr size_t max_bound_textures = 16u;

    // 0
    Vector4 albedo;

    // 16
    float metalness;
    float roughness;
    UInt32 param_0; // vec4 of 0.0..1.0 values stuffed into uint32
    UInt32 param_1; // vec4 of 0.0..1.0 values stuffed into uint32

    // 32
    UInt32 uv_flip_s;
    UInt32 uv_flip_t;
    float uv_scale;
    float parallax_height;

    // 48
    UInt32 texture_index[16];

    // 112
    UInt32 texture_usage;
    UInt32 _pad0;
    UInt32 _pad1;
    UInt32 _pad2;
    // 128
};

static_assert(sizeof(MaterialShaderData) == 128);

struct alignas(256) SceneShaderData {
    static constexpr UInt32 max_environment_textures = 1;

    Matrix4 view;
    Matrix4 projection;
    Vector4 camera_position;
    Vector4 camera_direction;
    Vector4 light_direction;

    UInt32 environment_texture_index;
    UInt32 environment_texture_usage;
    UInt32 resolution_x;
    UInt32 resolution_y;
    
    Vector4 aabb_max;
    Vector4 aabb_min;

    float   global_timer;
    UInt32  num_environment_shadow_maps;
    UInt32  num_lights;

    float camera_near;
    float camera_far;
};

static_assert(sizeof(SceneShaderData) == 256);

struct alignas(256) LightShaderData {
    Vector4  position; //direction for directional lights
    UInt32   color;
    UInt32   light_type;
    float    intensity;
    float    radius;
    UInt32   shadow_map_index; // ~0 == no shadow map
};

static_assert(sizeof(LightShaderData) == 256);

struct alignas(16) ShadowShaderData {
    Matrix4 projection;
    Matrix4 view;
    UInt32  scene_index;
};

//static_assert(sizeof(ShadowShaderData) == 128);

struct alignas(16) EnvProbeShaderData {
    Vector4 aabb_max;
    Vector4 aabb_min;
    Vector4 world_position;
    UInt32  texture_index;
};

/* max number of skeletons, based on size in mb */
constexpr size_t max_skeletons = (8ull * 1024ull * 1024ull) / sizeof(SkeletonShaderData);
constexpr size_t max_skeletons_bytes = max_skeletons * sizeof(SkeletonShaderData);
/* max number of materials, based on size in mb */
constexpr size_t max_materials = (8ull * 1024ull * 1024ull) / sizeof(MaterialShaderData);
constexpr size_t max_materials_bytes = max_materials * sizeof(MaterialShaderData);
/* max number of objects, based on size in mb */
constexpr size_t max_objects = (32ull * 1024ull * 1024ull) / sizeof(ObjectShaderData);
constexpr size_t max_objects_bytes = max_materials * sizeof(ObjectShaderData);
/* max number of scenes (cameras, essentially), based on size in kb */
constexpr size_t max_scenes = (32ull * 1024ull) / sizeof(SceneShaderData);
constexpr size_t max_scenes_bytes = max_scenes * sizeof(SceneShaderData);
/* max number of lights, based on size in kb */
constexpr size_t max_lights = (16ull * 1024ull) / sizeof(LightShaderData);
constexpr size_t max_lights_bytes = max_lights * sizeof(LightShaderData);
/* max number of shadow maps, based on size in kb */
constexpr size_t max_shadow_maps = (16ull * 1024ull) / sizeof(ShadowShaderData);
constexpr size_t max_shadow_maps_bytes = max_shadow_maps * sizeof(ShadowShaderData);

template <class Buffer, class StructType, size_t Size>
class ShaderData {
public:
    ShaderData(size_t num_buffers)
    {
        m_buffers.resize(num_buffers);

        for (size_t i = 0; i < num_buffers; i++) {
            m_buffers[i] = std::make_unique<Buffer>();
        }

        for (size_t i = 0; i < m_staging_objects_pool.num_staging_buffers; i++) {
            auto &buffer = m_staging_objects_pool.buffers[i];

            buffer.dirty.resize(num_buffers);

            for (auto &d : buffer.dirty) {
                d.SetStart(0);
                d.SetEnd(Size);
            }
        }
    }

    ShaderData(const ShaderData &other) = delete;
    ShaderData &operator=(const ShaderData &other) = delete;
    ~ShaderData() = default;

    const auto &GetBuffers() const { return m_buffers; }
    
    void Create(Device *device)
    {
        for (size_t i = 0; i < m_buffers.size(); i++) {
            HYPERION_ASSERT_RESULT(m_buffers[i]->Create(device, sizeof(StructType) * Size));
        }
    }

    void Destroy(Device *device)
    {
        for (size_t i = 0; i < m_buffers.size(); i++) {
            HYPERION_ASSERT_RESULT(m_buffers[i]->Destroy(device));
        }
    }

    void UpdateBuffer(Device *device, size_t buffer_index)
    {
#if HYP_BUFFERS_USE_SPINLOCK
        static constexpr UInt32 max_spins = 2;

        for (UInt32 spin_count = 0; spin_count < max_spins; spin_count++) {
            auto &current = m_staging_objects_pool.Current();

            if (!current.locked) {
                current.PerformUpdate(
                    device,
                    m_buffers,
                    buffer_index,
                    current.objects.Data()
                );
                
                m_staging_objects_pool.Next();

                return;
            }

            m_staging_objects_pool.Next();
        }
        
        DebugLog(
            LogType::Warn,
            "Buffer update spinlock exceeded maximum of %lu -- for %s\n",
            max_spins,
            typeid(StructType).name()
        );
#else
        auto &current = m_staging_objects_pool.Current();

        current.PerformUpdate(
            device,
            m_buffers,
            buffer_index,
            current.objects.Data()
        );
#endif
    }

    void Set(size_t index, const StructType &value)
    {
        m_staging_objects_pool.Set(index, value);
    }

    /*! \brief Get a reference to an object in the _current_ staging buffer,
     * use when it is preferable to fetch the object, update the struct, and then
     * call Set. This is usually when the object would have a large stack size
     */
    StructType &Get(size_t index)
    {
        return m_staging_objects_pool.Current().objects[index];
    }
    
private:
    struct StagingObjectsPool {
        static constexpr UInt32 num_staging_buffers = HYP_BUFFERS_USE_SPINLOCK ? 2 : 1;

        StagingObjectsPool() = default;
        StagingObjectsPool(const StagingObjectsPool &other) = delete;
        StagingObjectsPool &operator=(const StagingObjectsPool &other) = delete;
        StagingObjectsPool(StagingObjectsPool &other) = delete;
        StagingObjectsPool &operator=(StagingObjectsPool &&other) = delete;
        ~StagingObjectsPool() = default;

        struct StagingObjects {
            std::atomic_bool            locked{false};
            HeapArray<StructType, Size> objects;
            std::vector<Range<size_t>>  dirty;

            StagingObjects() = default;
            StagingObjects(const StagingObjects &other) = delete;
            StagingObjects &operator=(const StagingObjects &other) = delete;
            StagingObjects(StagingObjects &other) = delete;
            StagingObjects &operator=(StagingObjects &&other) = delete;
            ~StagingObjects() = default;

            template <class BufferContainer>
            void PerformUpdate(Device *device, BufferContainer &buffer_container, size_t buffer_index, StructType *ptr)
            {
                auto &current_dirty = dirty[buffer_index];

                const auto dirty_end   = current_dirty.GetEnd(),
                           dirty_start = current_dirty.GetStart();

                if (dirty_end <= dirty_start) {
                    return;
                }

                const auto dirty_distance = dirty_end - dirty_start;

                buffer_container[buffer_index]->Copy(
                    device,
                    dirty_start    * sizeof(StructType),
                    dirty_distance * sizeof(StructType),
                    &ptr[dirty_start]
                );

                current_dirty.Reset();
            }

            void MarkDirty(size_t index)
            {
                for (auto &d : dirty) {
                    d |= Range<size_t>{index, index + 1};
                }
            }

        } buffers[num_staging_buffers];
        
#if HYP_BUFFERS_USE_SPINLOCK
        std::atomic_uint32_t current_index{0};
#endif

        StagingObjects &Current()
        {

#if HYP_BUFFERS_USE_SPINLOCK
            return buffers[current_index];
#else
            return buffers[0];
#endif
        }

#if HYP_BUFFERS_USE_SPINLOCK
        void Next()
        {
            current_index = (current_index + 1) % num_staging_buffers;
        }
#endif

        void Set(size_t index, const StructType &value)
        {
            AssertThrowMsg(index < buffers[0].objects.Size(), "Cannot set shader data at %llu in buffer: out of bounds", index);

#if HYP_BUFFERS_USE_SPINLOCK
            for (UInt32 i = 0; i < num_staging_buffers; i++) {
                const auto staging_object_index = (current_index + i) % num_staging_buffers;
                auto &staging_object = buffers[staging_object_index];

                staging_object.locked = true;
                staging_object.objects[index] = value;
                staging_object.MarkDirty(index);
                staging_object.locked = false;
            }
#else
            auto &staging_object = buffers[0];
            
            staging_object.objects[index] = value;
            staging_object.MarkDirty(index);
#endif
        }
    };

    std::vector<std::unique_ptr<Buffer>> m_buffers;
    StagingObjectsPool m_staging_objects_pool;
};

} // namespace hyperion::v2

#endif
