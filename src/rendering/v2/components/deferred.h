#ifndef HYPERION_V2_DEFERRED_H
#define HYPERION_V2_DEFERRED_H

#include "post_fx.h"
#include "renderer.h"

namespace hyperion::v2 {

class DeferredRenderingEffect : public PostEffect {
public:
    DeferredRenderingEffect();
    DeferredRenderingEffect(const DeferredRenderingEffect &other) = delete;
    DeferredRenderingEffect &operator=(const DeferredRenderingEffect &other) = delete;
    ~DeferredRenderingEffect();

    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void Create(Engine *engine);

    void Destroy(Engine *engine);
    void Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index);
};

class DeferredRenderer : public Renderer {
public:
    DeferredRenderer();
    DeferredRenderer(const DeferredRenderer &other) = delete;
    DeferredRenderer &operator=(const DeferredRenderer &other) = delete;
    ~DeferredRenderer();

    inline DeferredRenderingEffect &GetEffect() { return m_effect; }
    inline const DeferredRenderingEffect &GetEffect() const { return m_effect; }

    void Create(Engine *engine);
    void CreateRenderList(Engine *engine);
    void CreatePipeline(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index);

private:

    DeferredRenderingEffect m_effect;
};

} // namespace hyperion::v2

#endif