#ifndef HYPERION_V2_FXAA_H
#define HYPERION_V2_FXAA_H

#include <rendering/post_fx.h>

namespace hyperion::v2 {

class FxaaEffect : public PostProcessingEffect {
public:
    static constexpr Stage stage = Stage::POST_SHADING;
    static constexpr uint  index = ~0;

    FxaaEffect();
    virtual ~FxaaEffect();

protected:
    virtual Ref<Shader> CreateShader(Engine *engine) override;
};

} // namespace hyperion::v2

#endif