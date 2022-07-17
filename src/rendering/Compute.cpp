#include "Compute.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {
ComputePipeline::ComputePipeline(Ref<Shader> &&shader)
    : EngineComponentBase(),
      m_pipeline(std::make_unique<renderer::ComputePipeline>()),
      m_shader(std::move(shader))
{
}

ComputePipeline::ComputePipeline(Ref<Shader> &&shader, const DynArray<const DescriptorSet *> &used_descriptor_sets)
    : EngineComponentBase(),
      m_pipeline(std::make_unique<renderer::ComputePipeline>(used_descriptor_sets)),
      m_shader(std::move(shader))
{
}

ComputePipeline::~ComputePipeline()
{
    Teardown();
}

void ComputePipeline::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_COMPUTE_PIPELINES, [this](...) {
        auto *engine = GetEngine();

        AssertThrow(m_shader != nullptr);
        m_shader->Init(engine);

        engine->render_scheduler.Enqueue([this, engine](...) {
           return m_pipeline->Create(
               engine->GetDevice(),
               m_shader->GetShaderProgram(),
               &engine->GetInstance()->GetDescriptorPool()
           ); 
        });

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_COMPUTE_PIPELINES, [this](...) {
            auto *engine = GetEngine();

            SetReady(false);

            engine->render_scheduler.Enqueue([this, engine](...) {
               return m_pipeline->Destroy(engine->GetDevice()); 
            });
            
            HYP_FLUSH_RENDER_QUEUE(engine);
        }));
    }));
}

} // namespace hyperion::v2
