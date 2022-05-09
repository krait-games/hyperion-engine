#ifndef HYPERION_V2_BUFFERS_H
#define HYPERION_V2_BUFFERS_H

#include <rendering/v2/core/lib/heap_array.h>
#include <rendering/v2/core/lib/range.h>

#include <memory>
#include <atomic>

namespace hyperion::v2 {

using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::StorageBuffer;
using renderer::PerFrameData;

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
        state |= uint32_t(value);

        return *this;
    }

    ShaderDataState &operator&=(State value)
    {
        state &= uint32_t(value);

        return *this;
    }

    bool IsClean() const { return state == CLEAN; }
    bool IsDirty() const { return state == DIRTY; }

private:
    uint32_t state;
};

struct alignas(256) SkeletonShaderData {
    static constexpr size_t max_bones = 128;

    Matrix4 bones[max_bones];
};

struct alignas(256) ObjectShaderData {
    Matrix4 model_matrix;
    uint32_t has_skinning;
    uint32_t material_index;
    uint32_t _padding[2];

    Vector4 local_aabb_max;
    Vector4 local_aabb_min;
    Vector4 world_aabb_max;
    Vector4 world_aabb_min;
};

static_assert(sizeof(ObjectShaderData) == 256);

struct alignas(256) MaterialShaderData {
    static constexpr size_t max_bound_textures = sizeof(uint32_t) * CHAR_BIT;

    Vector4 albedo;

    float metalness;
    float roughness;
    float subsurface;
    float specular;

    float specular_tint;
    float anisotropic;
    float sheen;
    float sheen_tint;

    float clearcoat;
    float clearcoat_gloss;
    float emissiveness;
    float _padding;

    uint32_t uv_flip_s;
    uint32_t uv_flip_t;
    float uv_scale;
    float parallax_height;

    uint32_t texture_index[max_bound_textures];
    uint32_t texture_usage;
    uint32_t _padding1;
    uint32_t _padding2;
};

static_assert(sizeof(MaterialShaderData) == 256);

struct alignas(256) SceneShaderData {
    static constexpr uint32_t max_environment_textures = 1;

    Matrix4 view;
    Matrix4 projection;
    Vector4 camera_position;
    Vector4 camera_direction;
    Vector4 light_direction;

    uint32_t environment_texture_index;
    uint32_t environment_texture_usage;
    uint32_t resolution_x;
    uint32_t resolution_y;
    
    Vector4 aabb_max;
    Vector4 aabb_min;
};

static_assert(sizeof(SceneShaderData) == 256);

struct alignas(16) LightShaderData {
    Vector4  position; //direction for directional lights
    uint32_t color;
    uint32_t light_type;
    float    intensity;
};

static_assert(sizeof(LightShaderData) == 32);

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
            m_buffers[i]->Create(device, sizeof(StructType) * Size);
        }
    }

    void Destroy(Device *device)
    {
        for (size_t i = 0; i < m_buffers.size(); i++) {
            m_buffers[i]->Destroy(device);
        }
    }

    void UpdateBuffer(Device *device, size_t buffer_index)
    {
        static constexpr uint32_t max_spins = 2;

        for (uint32_t spin_count = 0; spin_count < max_spins; spin_count++) {
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
        static constexpr uint32_t num_staging_buffers = 2;

        struct StagingObjects {
            std::atomic_bool                 locked{false};
            HeapArray<StructType, Size>      objects;
            std::vector<AtomicRange<size_t>> dirty;

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

                current_dirty.SetStart(UINT32_MAX);
                current_dirty.SetEnd(0);
            }

            void MarkDirty(size_t index)
            {
                for (auto &d : dirty) {
                    d |= Range<size_t>{index, index + 1};
                }
            }

        } buffers[num_staging_buffers];
        
        std::atomic_uint32_t current_index{0};

        StagingObjects &Current()
        {
            return buffers[current_index];
        }

        void Next()
        {
            current_index = (current_index + 1) % num_staging_buffers;
        }

        void Set(size_t index, const StructType &value)
        {
            AssertThrowMsg(index < buffers[0].objects.Size(), "Cannot set shader data out of bounds");
            
            for (uint32_t i = 0; i < num_staging_buffers; i++) {
                const auto staging_object_index = (current_index + i + 1) % num_staging_buffers;
                auto &staging_object = buffers[staging_object_index];

                staging_object.locked = true;
                staging_object.objects[index] = value;
                staging_object.MarkDirty(index);
                staging_object.locked = false;
            }
        }
    };

    std::vector<std::unique_ptr<Buffer>> m_buffers;
    StagingObjectsPool m_staging_objects_pool;
};

} // namespace hyperion::v2

#endif