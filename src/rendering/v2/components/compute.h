#ifndef HYPERION_V2_COMPUTE_H
#define HYPERION_V2_COMPUTE_H

#include "shader.h"

#include <rendering/backend/renderer_command_buffer.h>
#include <rendering/backend/renderer_compute_pipeline.h>

#include <memory>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::IndirectBuffer;

class ComputePipeline : public EngineComponent<renderer::ComputePipeline> {
public:
    explicit ComputePipeline(Ref<Shader> &&shader);
    ComputePipeline(const ComputePipeline &) = delete;
    ComputePipeline &operator=(const ComputePipeline &) = delete;
    ~ComputePipeline();

    ENGINE_COMPONENT_DELEGATE_METHODS

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    /*void Dispatch(
        Engine *engine,
        CommandBuffer *command_buffer,
        Extent3D group_size);

    void DispatchIndirect(
        Engine *engine,
        CommandBuffer *command_buffer,
        IndirectBuffer *indirect,
        size_t offset = 0);*/

private:
    /*void BindDescriptorSets(
        Engine *engine,
        CommandBuffer *command_buffer);*/

    Ref<Shader> m_shader;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_COMPUTE_H

