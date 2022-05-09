#ifndef HYPERION_V2_FRAMEBUFFER_H
#define HYPERION_V2_FRAMEBUFFER_H

#include "base.h"
#include "render_pass.h"

#include <rendering/backend/renderer_fbo.h>
#include <rendering/backend/renderer_command_buffer.h>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::Extent2D;

class Framebuffer : public EngineComponentBase<STUB_CLASS(Framebuffer)> {
public:
    Framebuffer(Extent2D extent, Ref<RenderPass> &&render_pass);
    Framebuffer(const Framebuffer &other) = delete;
    Framebuffer &operator=(const Framebuffer &other) = delete;
    ~Framebuffer();

    renderer::FramebufferObject &GetFramebuffer()             { return m_framebuffer; }
    const renderer::FramebufferObject &GetFramebuffer() const { return m_framebuffer; }

    RenderPass *GetRenderPass() const { return m_render_pass.ptr; }

    void Init(Engine *engine);

    void BeginCapture(CommandBuffer *command_buffer);
    void EndCapture(CommandBuffer *command_buffer);

private:
    renderer::FramebufferObject m_framebuffer;
    Ref<RenderPass>             m_render_pass;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_FRAMEBUFFER_H
