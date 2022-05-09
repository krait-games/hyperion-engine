#ifndef HYPERION_V2_PROBE_SYSTEM_H
#define HYPERION_V2_PROBE_SYSTEM_H

#include "shader.h"
#include "compute.h"

#include <rendering/backend/rt/renderer_raytracing_pipeline.h>
#include <rendering/backend/renderer_image.h>

#include <math/bounding_box.h>

#include <random>

namespace hyperion::v2 {

using renderer::RaytracingPipeline;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::UniformBuffer;
using renderer::CommandBuffer;

class Engine;

struct alignas(256) ProbeSystemUniforms {
    Vector4  aabb_max;
    Vector4  aabb_min;
    Extent3D probe_border;
    Extent3D probe_counts;
    Extent2D image_dimensions;
    Extent2D irradiance_image_dimensions;
    Extent2D depth_image_dimensions;
    float    probe_distance;
    uint32_t num_rays_per_probe;
};

struct alignas(16) ProbeRayData {
    Vector4 direction_depth;
    Vector4 rigin;
    Vector4 normal;
    Vector4 color;
};

static_assert(sizeof(ProbeRayData) == 64);

struct ProbeSystemSetup {
    static constexpr uint32_t num_rays_per_probe         = 128;
    static constexpr uint32_t irradiance_octahedron_size = 8;
    static constexpr uint32_t depth_octahedron_size      = 16;

    BoundingBox aabb;
    Extent3D    probe_border   = {2, 0, 2};
    float       probe_distance = 2.0f;

    const Vector3 &GetOrigin() const
        { return aabb.min; }

    Extent3D NumProbesPerDimension() const
    {
        const auto probes_per_dimension = MathUtil::Ceil((aabb.GetDimensions() / probe_distance) + probe_border.ToVector3());

        return {
            static_cast<decltype(Extent3D::width)>(probes_per_dimension.x),
            static_cast<decltype(Extent3D::height)>(probes_per_dimension.y),
            static_cast<decltype(Extent3D::depth)>(probes_per_dimension.z)
        };
    }

    uint32_t NumProbes() const
    {
        const Extent3D per_dimension = NumProbesPerDimension();

        return per_dimension.width * per_dimension.height * per_dimension.depth;
    }

    Extent2D GetImageDimensions() const
    {
        return Extent2D(MathUtil::NextPowerOf2(NumProbes()), num_rays_per_probe);
    }
};

struct RotationMatrixGenerator {
    Matrix4                               matrix;
    std::random_device                    random_device;
    std::mt19937                          mt{random_device()};
    std::uniform_real_distribution<float> angle{0.0f, 360.0f};
    std::uniform_real_distribution<float> axis{-1.0f, 1.0f};

    const Matrix4 &Next()
    {
        return matrix = Matrix4::Rotation({
            Vector3{axis(mt), axis(mt), axis(mt)}.Normalize(),
            MathUtil::DegToRad(angle(mt))
        });
    }
};

struct Probe {
    Vector3 position;
};

class ProbeSystem {
public:
    ProbeSystem(ProbeSystemSetup &&setup);
    ProbeSystem(const ProbeSystem &other) = delete;
    ProbeSystem &operator=(const ProbeSystem &other) = delete;
    ~ProbeSystem();

    inline StorageBuffer *GetRadianceBuffer() const  { return m_radiance_buffer.get(); }
    inline StorageImage *GetIrradianceImage() const  { return m_irradiance_image.get(); }
    inline ImageView *GetIrradianceImageView() const { return m_irradiance_image_view.get(); }

    void Init(Engine *engine);
    void Destroy(Engine *engine);

    void RenderProbes(Engine *engine, CommandBuffer *command_buffer);
    void ComputeIrradiance(Engine *engine, CommandBuffer *command_buffer);

private:
    void CreatePipeline(Engine *engine);
    void CreateComputePipelines(Engine *engine);
    void CreateUniformBuffer(Engine *engine);
    void CreateStorageBuffers(Engine *engine);
    void AddDescriptors(Engine *engine);
    void SubmitPushConstants(Engine *engine, CommandBuffer *command_buffer);

    ProbeSystemSetup   m_setup;
    std::vector<Probe> m_probes;

    Ref<ComputePipeline> m_update_irradiance,
                         m_update_depth;

    std::unique_ptr<RaytracingPipeline> m_pipeline;
    std::unique_ptr<UniformBuffer>      m_uniform_buffer;

    std::unique_ptr<StorageBuffer>      m_radiance_buffer;

    std::unique_ptr<StorageImage>       m_irradiance_image;
    std::unique_ptr<ImageView>          m_irradiance_image_view;

    std::unique_ptr<StorageImage>       m_depth_image;
    std::unique_ptr<ImageView>          m_depth_image_view;

    RotationMatrixGenerator m_random_generator;
    uint32_t                m_time;
};

} // namespace hyperion::v2

#endif