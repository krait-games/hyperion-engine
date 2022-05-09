#include "light.h"
#include "../engine.h"

#include <util/byte_util.h>

namespace hyperion::v2 {

Light::Light(
    LightType type,
    const Vector3 &position,
    const Vector4 &color,
    float intensity
) : m_type(type),
    m_position(position),
    m_color(color),
    m_intensity(intensity),
    m_shader_data_state(ShaderDataState::DIRTY)
{
}

Light::~Light()
{
    Teardown();
}

void Light::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_LIGHTS, [this](Engine *engine) {
        UpdateShaderData(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_LIGHTS, [this](Engine *engine) {
            /* no-op */
        }), engine);
    }));
}

void Light::UpdateShaderData(Engine *engine) const
{
    std::lock_guard guard(engine->render_mutex);

    LightShaderData shader_data{
        .position   = Vector4(m_position, 1.0f),
        .color      = ByteUtil::PackColorU32(m_color),
        .light_type = static_cast<uint32_t>(m_type),
        .intensity  = m_intensity
    };
    
    engine->shader_globals->lights.Set(m_id.value - 1, std::move(shader_data));

    m_shader_data_state = ShaderDataState::CLEAN;
}


} // namespace hyperion::v2