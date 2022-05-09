#include "tlas.h"
#include "../engine.h"

namespace hyperion::v2 {

Tlas::Tlas()
    : EngineComponent()
{
}

Tlas::~Tlas()
{
    Teardown();
}

void Tlas::AddBlas(Ref<Blas> &&blas)
{
    if (blas == nullptr) {
        return;
    }

    if (IsInit()) {
        blas.Init();
    }

    m_blas.push_back(std::move(blas));
}

void Tlas::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ACCELERATION_STRUCTURES, [this](Engine *engine) {
        std::vector<BottomLevelAccelerationStructure *> blas(m_blas.size());

        for (size_t i = 0; i < m_blas.size(); i++) {
            AssertThrow(m_blas[i] != nullptr);

            m_blas[i].Init();
            blas[i] = &m_blas[i]->Get();
        }

        EngineComponent::Create(
            engine,
            engine->GetInstance(),
            std::move(blas)
        );

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ACCELERATION_STRUCTURES, [this](Engine *engine) {
            m_blas.clear();

            EngineComponent::Destroy(engine);
        }), engine);
    }));
}

} // namespace hyperion::v2