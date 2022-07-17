#ifndef HYPERION_V2_COMPUTE_H
#define HYPERION_V2_COMPUTE_H

#include "Shader.hpp"

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::IndirectBuffer;
using renderer::DescriptorSet;

class ComputePipeline : public EngineComponentBase<STUB_CLASS(ComputePipeline)> {
public:
    ComputePipeline(Ref<Shader> &&shader);
    ComputePipeline(Ref<Shader> &&shader, const DynArray<const DescriptorSet *> &used_descriptor_sets);
    ComputePipeline(const ComputePipeline &) = delete;
    ComputePipeline &operator=(const ComputePipeline &) = delete;
    ~ComputePipeline();

    renderer::ComputePipeline *GetPipeline() const { return m_pipeline.get(); }

    void Init(Engine *engine);

private:
    std::unique_ptr<renderer::ComputePipeline> m_pipeline;
    Ref<Shader> m_shader;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_COMPUTE_H

