#include "Atomics.hpp"
#include "../Engine.hpp"
#include <Types.hpp>

namespace hyperion::v2 {

using Context = renderer::StagingBufferPool::Context;

AtomicCounter::AtomicCounter() = default;

AtomicCounter::~AtomicCounter()
{
    AssertThrowMsg(m_buffer == nullptr, "buffer should have been destroyed before destructor call");
}

void AtomicCounter::Create(Engine *engine)
{
    AssertThrow(m_buffer == nullptr);

    m_buffer = std::make_unique<AtomicCounterBuffer>();
    HYPERION_ASSERT_RESULT(m_buffer->Create(engine->GetInstance()->GetDevice(), sizeof(UInt32)));
}

void AtomicCounter::Destroy(Engine *engine)
{
    AssertThrow(m_buffer != nullptr);

    HYPERION_ASSERT_RESULT(m_buffer->Destroy(engine->GetInstance()->GetDevice()));
    m_buffer.reset();
}

void AtomicCounter::Reset(Engine *engine, CountType value)
{
    HYPERION_ASSERT_RESULT(engine->GetInstance()->GetStagingBufferPool().Use(
        engine->GetInstance()->GetDevice(),
        [this, engine, value](Context &context) {
            auto *staging_buffer = context.Acquire(sizeof(value));
            
            staging_buffer->Copy(engine->GetInstance()->GetDevice(), sizeof(value), (void *)&value);
   
            auto commands = engine->GetInstance()->GetSingleTimeCommands();

            commands.Push([&](CommandBuffer *command_buffer) {
                m_buffer->CopyFrom(command_buffer, staging_buffer, sizeof(value));

                HYPERION_RETURN_OK;
            });

            return commands.Execute(engine->GetInstance()->GetDevice());
        }
    ));
}

auto AtomicCounter::Read(Engine *engine) const -> CountType
{
    auto result = MathUtil::MaxSafeValue<CountType>();

    HYPERION_ASSERT_RESULT(engine->GetInstance()->GetStagingBufferPool().Use(
        engine->GetInstance()->GetDevice(),
        [this, engine, &result](Context &context) {
            auto *staging_buffer = context.Acquire(sizeof(result));
            
            auto commands = engine->GetInstance()->GetSingleTimeCommands();

            commands.Push([&](CommandBuffer *command_buffer) {
                staging_buffer->CopyFrom(command_buffer, m_buffer.get(), sizeof(result));

                HYPERION_RETURN_OK;
            });

            HYPERION_BUBBLE_ERRORS(commands.Execute(engine->GetInstance()->GetDevice()));
            
            staging_buffer->Read(engine->GetInstance()->GetDevice(), sizeof(result), (void *)&result);

            HYPERION_RETURN_OK;
        }
    ));

    return result;
}

} // namespace hyperion::v2