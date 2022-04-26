#ifndef HYPERION_V2_MIXINS_H
#define HYPERION_V2_MIXINS_H

#include <rendering/backend/rt/renderer_acceleration_structure.h>
#include <rendering/backend/renderer_result.h>

namespace hyperion::v2::mixins {

using renderer::Result;

template <class T, class AS = renderer::BottomLevelAccelerationStructure>
class HasAccelerationStructure {
public:
    AS *GetAccelerationStructure() const { return m_acceleration_structure.get(); }

protected:
    Result Create(Device *device)
    {
        return m_acceleration_structure->Create(device);
    }

    Result Destroy(Device *device)
    {
        return m_acceleration_structure->Destroy(device);
    }

    std::unique_ptr<AS> m_acceleration_structure = std::make_unique<AS>();
};

} // namespace hyperion::v2::mixins

#endif