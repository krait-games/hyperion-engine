#ifndef HYPERION_V2_ATOMICS_H
#define HYPERION_V2_ATOMICS_H

#include <rendering/backend/renderer_command_buffer.h>
#include <rendering/backend/renderer_fence.h>
#include <rendering/backend/renderer_buffer.h>

namespace hyperion::v2 {

using renderer::Fence;
using renderer::CommandBuffer;
using renderer::GPUBuffer;
using renderer::AtomicCounterBuffer;
using renderer::StagingBuffer;

class Engine;

class AtomicCounter {
public:
    AtomicCounter();
    AtomicCounter(const AtomicCounter &other) = delete;
    AtomicCounter &operator=(const AtomicCounter &other) = delete;
    ~AtomicCounter();

    AtomicCounterBuffer *GetBuffer() const { return m_buffer.get(); }

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void Reset(Engine *engine, uint32_t value = 0);
    uint32_t Read(Engine *engine) const;

private:
    std::unique_ptr<AtomicCounterBuffer> m_buffer;
    std::unique_ptr<StagingBuffer> m_staging_buffer;
};

} // namespace hyperion::v2

#endif