#ifndef HYPERION_RENDERER_STRUCTS_H
#define HYPERION_RENDERER_STRUCTS_H

#include <util/EnumOptions.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/Matrix4.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>
#include <algorithm>
#include <optional>
#include <set>
#include <memory>
#include <utility>
#include <tuple>

namespace hyperion {
namespace renderer {

enum class DatumType {
    UNSIGNED_BYTE,
    SIGNED_BYTE,
    UNSIGNED_SHORT,
    SIGNED_SHORT,
    UNSIGNED_INT,
    SIGNED_INT,
    FLOAT
};

enum class FaceCullMode {
    NONE,
    BACK,
    FRONT
};

enum class FillMode {
    FILL,
    LINE
};

enum class Topology {
    TRIANGLES,
    TRIANGLE_FAN,
    TRIANGLE_STRIP,

    LINES,

    POINTS
};

enum class StencilMode {
    NONE,
    FILL,
    OUTLINE
};

struct StencilState {
    UInt        id   = 0;
    StencilMode mode = StencilMode::NONE;

    HYP_DEF_STRUCT_COMPARE_EQL(StencilState);

    bool operator<(const StencilState &other) const
    {
        return std::tie(id, mode) < std::tie(other.id, other.mode);
    }
};

struct MeshBindingDescription {
    UInt32            binding;
    UInt32            stride;
    VkVertexInputRate input_rate;

    MeshBindingDescription()
        : binding(0), stride(0), input_rate(VK_VERTEX_INPUT_RATE_VERTEX)
    {
    }

    MeshBindingDescription(UInt32 binding, UInt32 stride, VkVertexInputRate input_rate)
        : binding(binding), stride(stride), input_rate(input_rate)
    {
    }

    VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription bind;
        bind.binding = this->binding;
        bind.stride = this->stride;
        bind.inputRate = this->input_rate;
        return bind;
    }
};

struct VertexAttribute {
    enum Type {
        MESH_INPUT_ATTRIBUTE_UNDEFINED    = 0,
        MESH_INPUT_ATTRIBUTE_POSITION     = 1,
        MESH_INPUT_ATTRIBUTE_NORMAL       = 2,
        MESH_INPUT_ATTRIBUTE_TEXCOORD0    = 4,
        MESH_INPUT_ATTRIBUTE_TEXCOORD1    = 8,
        MESH_INPUT_ATTRIBUTE_TANGENT      = 16,
        MESH_INPUT_ATTRIBUTE_BITANGENT    = 32,
        MESH_INPUT_ATTRIBUTE_BONE_INDICES = 64,
        MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS = 128,
    };

    static const EnumOptions<Type, VertexAttribute, 16> mapping;

    UInt32      location;
    UInt32      binding;
    // total size -- num elements * sizeof(float)
    size_t      size;

    inline bool operator<(const VertexAttribute &other) const
        { return location < other.location; }

    inline VkFormat GetFormat() const
    {
        switch (this->size) {
        case sizeof(float):     return VK_FORMAT_R32_SFLOAT;
        case sizeof(float) * 2: return VK_FORMAT_R32G32_SFLOAT;
        case sizeof(float) * 3: return VK_FORMAT_R32G32B32_SFLOAT;
        case sizeof(float) * 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        default: AssertThrowMsg(0, "Unsupported vertex attribute format!");
        }
    }

    VkVertexInputAttributeDescription GetAttributeDescription() const {
        VkVertexInputAttributeDescription attrib{};
        attrib.location = this->location;
        attrib.binding = this->binding;
        attrib.format = this->GetFormat();

        return attrib;
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(this->location);
        hc.Add(this->binding);
        hc.Add(this->size);

        return hc;
    }
};

struct VertexAttributeSet {
    UInt64 flag_mask;

    constexpr VertexAttributeSet()
        : flag_mask(0) {}
    constexpr VertexAttributeSet(UInt64 flag_mask)
        : flag_mask(flag_mask) {}
    constexpr VertexAttributeSet(VertexAttribute::Type flags)
        : flag_mask(static_cast<UInt64>(flags)) {}
    constexpr VertexAttributeSet(const VertexAttributeSet &other)
        : flag_mask(other.flag_mask) {}

    VertexAttributeSet &operator=(const VertexAttributeSet &other)
    {
        flag_mask = other.flag_mask;

        return *this;
    }

    ~VertexAttributeSet() = default;

    explicit operator bool() const { return bool(flag_mask); }

    bool operator==(const VertexAttributeSet &other) const              { return flag_mask == other.flag_mask; }
    bool operator!=(const VertexAttributeSet &other) const              { return flag_mask != other.flag_mask; }

    bool operator==(UInt64 flags) const                                 { return flag_mask == flags; }
    bool operator!=(UInt64 flags) const                                 { return flag_mask != flags; }

    VertexAttributeSet operator~() const                                { return ~flag_mask; }

    VertexAttributeSet operator&(const VertexAttributeSet &other) const { return {flag_mask & other.flag_mask}; }
    VertexAttributeSet &operator&=(const VertexAttributeSet &other)     { flag_mask &= other.flag_mask; return *this; }
    VertexAttributeSet operator&(UInt64 flags) const                    { return {flag_mask & flags}; }
    VertexAttributeSet &operator&=(UInt64 flags)                        { flag_mask &= flags; return *this; }
    
    VertexAttributeSet operator|(const VertexAttributeSet &other) const { return {flag_mask | other.flag_mask}; }
    VertexAttributeSet &operator|=(const VertexAttributeSet &other)     { flag_mask |= other.flag_mask; return *this; }
    VertexAttributeSet operator|(UInt64 flags) const                    { return {flag_mask | flags}; }
    VertexAttributeSet &operator|=(UInt64 flags)                        { flag_mask |= flags; return *this; }

    bool operator<(const VertexAttributeSet &other) const               { return flag_mask < other.flag_mask; }

    bool Has(VertexAttribute::Type type) const { return bool(operator&(UInt64(type))); }

    void Set(UInt64 flags, bool enable = true)
    {
        if (enable) {
            flag_mask |= flags;
        } else {
            flag_mask &= ~flags;
        }
    }

    void Set(VertexAttribute::Type type, bool enable = true)
    {
        Set(UInt64(type), enable);
    }

    void Merge(const VertexAttributeSet &other)
    {
        flag_mask |= other.flag_mask;
    }

    std::vector<VertexAttribute> BuildAttributes() const
    {
        std::vector<VertexAttribute> attributes;
        attributes.reserve(VertexAttribute::mapping.Size());

        for (size_t i = 0; i < VertexAttribute::mapping.Size(); i++) {
            const UInt64 iter_flag_mask = VertexAttribute::mapping.OrdinalToEnum(i);  // NOLINT(readability-static-accessed-through-instance)

            if (flag_mask & iter_flag_mask) {
                attributes.push_back(VertexAttribute::mapping[VertexAttribute::Type(iter_flag_mask)]);
            }
        }

        return attributes;
    }

    inline size_t CalculateVertexSize() const
    {
        size_t size = 0;

        for (size_t i = 0; i < VertexAttribute::mapping.Size(); i++) {
            const UInt64 iter_flag_mask = VertexAttribute::mapping.OrdinalToEnum(i);  // NOLINT(readability-static-accessed-through-instance)

            if (flag_mask & iter_flag_mask) {
                size += VertexAttribute::mapping[VertexAttribute::Type(iter_flag_mask)].size;
            }
        }

        return size;
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(flag_mask);

        return hc;
    }
};

constexpr VertexAttributeSet static_mesh_vertex_attributes(
    VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_TANGENT
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT
);

constexpr VertexAttributeSet skeleton_vertex_attributes(
    VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_INDICES
);


struct QueueFamilyIndices {
    using Index = UInt32;
    
    std::optional<Index> graphics_family;
    std::optional<Index> transfer_family;
    std::optional<Index> present_family;
    std::optional<Index> compute_family;

    bool IsComplete() const {
        return graphics_family.has_value()
            && transfer_family.has_value()
            && present_family.has_value()
            && compute_family.has_value();
    }
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR             capabilities;
    std::vector<VkQueueFamilyProperties> queue_family_properties;
    std::vector<VkSurfaceFormatKHR>      formats;
    std::vector<VkPresentModeKHR>        present_modes;
};

struct alignas(8) Extent2D {
    union {
        struct {  // NOLINT(clang-diagnostic-nested-anon-types)
            UInt32 width, height;
        };

        UInt32 v[2];
    };

    Extent2D()
        : width(0),
          height(0)
    {
    }

    Extent2D(UInt32 width, UInt32 height)
        : width(width),
          height(height)
    {
    }

    Extent2D(const Extent2D &other) = default;
    Extent2D &operator=(const Extent2D &other) = default;
    Extent2D(Extent2D &&other) noexcept = default;
    Extent2D &operator=(Extent2D &&other) noexcept = default;
    ~Extent2D() = default;

    bool operator==(const Extent2D &other) const
    {
        return width == other.width
            && height == other.height;
    }

    bool operator!=(const Extent2D &other) const
        { return !operator==(other); }
    
    constexpr UInt32 &operator[](UInt32 index)      { return v[index]; }
    constexpr UInt32 operator[](UInt32 index) const { return v[index]; }

    UInt32 Size() const { return width * height; }

    operator Vector2() const
    {
        return {
            static_cast<Float>(width),
            static_cast<Float>(height)
        };
    }
};

static_assert(sizeof(Extent2D) == 8);

struct alignas(16) Extent3D {
    union {
        struct {  // NOLINT(clang-diagnostic-nested-anon-types)
            UInt32 width, height, depth;
        };

        UInt32 v[3];
    };

    Extent3D()
        : width(0),
          height(0),
          depth(0)
    {
    }

    explicit Extent3D(UInt32 extent)
        : width(extent),
          height(extent),
          depth(extent)
    {
    }

    Extent3D(UInt32 width, UInt32 height, UInt32 depth)
        : width(width),
          height(height),
          depth(depth)
    {
    }

    explicit Extent3D(const Extent2D &extent_2d, UInt32 depth = 1)
        : width(extent_2d.width),
          height(extent_2d.height),
          depth(depth)
    {
    }

    Extent3D(const Extent3D &other) = default;
    Extent3D &operator=(const Extent3D &other) = default;
    Extent3D(Extent3D &&other) noexcept = default;
    Extent3D &operator=(Extent3D &&other) noexcept = default;
    ~Extent3D() = default;

    bool operator==(const Extent3D &other) const
    {
        return width == other.width
            && height == other.height
            && depth == other.depth;
    }

    bool operator!=(const Extent3D &other) const
        { return !operator==(other); }
    
    Extent3D operator*(const Extent3D &other) const { return Extent3D(width * other.width, height * other.height, depth * other.depth); }
    Extent3D &operator*=(const Extent3D &other)
    {
        width *= other.width;
        height *= other.height;
        depth *= other.depth;

        return *this;
    }

    Extent3D operator/(const Extent3D &other) const { return Extent3D(width / other.width, height / other.height, depth / other.depth); }
    Extent3D &operator/=(const Extent3D &other)
    {
        width  /= other.width;
        height /= other.height;
        depth  /= other.depth;

        return *this;
    }
    
    constexpr UInt32 &operator[](UInt32 index)         { return v[index]; }
    constexpr UInt32 operator[](UInt32 index) const    { return v[index]; }

    operator Extent2D() const
    {
        return {
            width,
            height
        };
    }
    
    operator Vector3() const
    {
        return {
            static_cast<Float>(width),
            static_cast<Float>(height),
            static_cast<Float>(depth)
        };
    }

    UInt32 Size() const { return width * height * depth; }
};

static_assert(sizeof(Extent3D) == 16);

template <class T>
struct alignas(4) ShaderVec2 {
    T x, y;

    ShaderVec2() = default;
    ShaderVec2(const ShaderVec2 &other) = default;
    ShaderVec2(T x, T y) : x(x), y(y) {}
    ShaderVec2(const Vector2 &vec)
        : x(vec.x),
          y(vec.y)
    {
    }
    ShaderVec2(const Extent2D &extent)
        : x(extent.width),
          y(extent.height)
    {
    }

    operator Vector2() const { return Vector2(x, y); }
};

static_assert(sizeof(ShaderVec2<Float>) == 8);
static_assert(sizeof(ShaderVec2<UInt32>) == 8);

template <class T>
struct alignas(4) ShaderVec3 {
    T x, y, z;

    ShaderVec3() = default;
    ShaderVec3(const ShaderVec3 &other) = default;
    ShaderVec3(T x, T y, T z) : x(x), y(y), z(z) {}
    ShaderVec3(const Vector3 &vec)
        : x(vec.x),
          y(vec.y),
          z(vec.z)
    {
    }
    ShaderVec3(const Extent3D &extent)
        : x(extent.width),
          y(extent.height),
          z(extent.depth)
    {
    }

    operator Vector3() const { return Vector3(x, y, z); }
};

static_assert(sizeof(ShaderVec3<float>)  == 12);
static_assert(sizeof(ShaderVec3<UInt32>) == 12);

template <class T>
struct alignas(4) ShaderVec4 {
    T x, y, z, w;

    ShaderVec4() = default;
    ShaderVec4(const ShaderVec4 &other) = default;
    ShaderVec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
    ShaderVec4(const Vector4 &vec)
        : x(vec.x),
          y(vec.y),
          z(vec.z),
          w(vec.w)
    {
    }

    operator Vector4() const { return Vector4(x, y, z, w); }
};

static_assert(sizeof(ShaderVec4<Float>)  == 16);
static_assert(sizeof(ShaderVec4<UInt32>) == 16);

struct alignas(4) ShaderMat4 {
    Float m00, m01, m02, m03,
          m10, m11, m12, m13,
          m20, m21, m22, m23,
          m30, m31, m32, m33;

    ShaderMat4() = default;
    ShaderMat4(const ShaderMat4 &other) = default;
    ShaderMat4(const Matrix4 &mat)
        : m00(mat[0][0]), m01(mat[0][1]), m02(mat[0][2]), m03(mat[0][3]),
          m10(mat[1][0]), m11(mat[1][1]), m12(mat[1][2]), m13(mat[1][3]),
          m20(mat[2][0]), m21(mat[2][1]), m22(mat[2][2]), m23(mat[2][3]),
          m30(mat[3][0]), m31(mat[3][1]), m32(mat[3][2]), m33(mat[3][3])
    {
    }

    operator Matrix4() const
    {
        const Float values[16] = {
            m00, m01, m02, m03,
            m10, m11, m12, m13,
            m20, m21, m22, m23,
            m30, m31, m32, m33
        };

        return Matrix4(&values[0]);
    }
};

static_assert(sizeof(ShaderMat4) == 64);

struct alignas(8) Rect {
    uint32_t x0, y0,
             x1, y1;
};

static_assert(sizeof(Rect) == 16);


struct PackedVertex {
    float position_x,
          position_y,
          position_z,
          normal_x,
          normal_y,
          normal_z,
          texcoord0_x,
          texcoord0_y;
};

static_assert(sizeof(PackedVertex) % 16 == 0);

struct MeshDescription {
    UInt64 vertex_buffer_address;
    UInt64 index_buffer_address;
};

static_assert(sizeof(MeshDescription) % 16 == 0);

using PackedIndex = UInt32;

using ImageSubResourceFlagBits = UInt;

enum ImageSubResourceFlags : ImageSubResourceFlagBits {
    IMAGE_SUB_RESOURCE_FLAGS_NONE    = 0,
    IMAGE_SUB_RESOURCE_FLAGS_COLOR   = 1 << 0,
    IMAGE_SUB_RESOURCE_FLAGS_DEPTH   = 1 << 1,
    IMAGE_SUB_RESOURCE_FLAGS_STENCIL = 1 << 2
};

/* images */
struct ImageSubResource {
    ImageSubResourceFlagBits flags = IMAGE_SUB_RESOURCE_FLAGS_COLOR;
    UInt32 base_array_layer        = 0;
    UInt32 base_mip_level          = 0;
    UInt32 num_layers              = 1;
    UInt32 num_levels              = 1;

    bool operator==(const ImageSubResource &other) const
    {
        return flags == other.flags
            && base_array_layer == other.base_array_layer
            && num_layers == other.num_layers
            && base_mip_level == other.base_mip_level
            && num_levels == other.num_levels;
    }
};

template<class ...Args>
class PerFrameData {
    struct FrameDataWrapper {
        std::tuple<std::unique_ptr<Args>...> tup;

        template <class T>
        T *Get()
        {
            return std::get<std::unique_ptr<T>>(tup).get();
        }

        template <class T>
        const T *Get() const
        {
            return std::get<std::unique_ptr<T>>(tup).get();
        }

        template <class T>
        void Set(std::unique_ptr<T> &&value)
        {
            std::get<std::unique_ptr<T>>(tup) = std::move(value);
        }
    };

public:
    PerFrameData(UInt32 num_frames) : m_num_frames(num_frames)
        { m_data.resize(num_frames); }

    PerFrameData(const PerFrameData &other) = delete;
    PerFrameData &operator=(const PerFrameData &other) = delete;
    PerFrameData(PerFrameData &&) = default;
    PerFrameData &operator=(PerFrameData &&) = default;
    ~PerFrameData() = default;

    inline UInt32 NumFrames() const
        { return m_num_frames; }

    inline FrameDataWrapper &operator[](UInt32 index)
        { return m_data[index]; }

    inline const FrameDataWrapper &operator[](UInt32 index) const
        { return m_data[index]; }

    inline FrameDataWrapper &At(UInt32 index)
        { return m_data[index]; }

    inline const FrameDataWrapper &At(UInt32 index) const
        { return m_data[index]; }

    inline void Reset()
        { m_data = std::vector<FrameDataWrapper>(m_num_frames); }

protected:
    UInt32 m_num_frames;
    std::vector<FrameDataWrapper> m_data;
};

struct alignas(4) IndirectDrawCommand {
    // native vk object
    VkDrawIndexedIndirectCommand command;

    HYP_PAD_STRUCT_HERE(Float32, 7);
};

static_assert(std::is_standard_layout_v<IndirectDrawCommand>, "IndirectDrawCommand must be POD");
static_assert(sizeof(IndirectDrawCommand) % 4 == 0, "IndirectDrawCommand must have a sizeof multiple of 4");

} // namespace renderer
} // namespace hyperion

template <>
struct ::std::hash<hyperion::renderer::ImageSubResource> {
    inline size_t operator()(const hyperion::renderer::ImageSubResource &sub_resource) const
    {
        hyperion::HashCode hc;
        hc.Add(sub_resource.flags);
        hc.Add(sub_resource.base_array_layer);
        hc.Add(sub_resource.num_layers);
        hc.Add(sub_resource.base_mip_level);
        hc.Add(sub_resource.num_levels);

        return hc.Value();
    }
};

#endif //HYPERION_RENDERER_STRUCTS_H
