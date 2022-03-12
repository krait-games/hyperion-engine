#ifndef HYPERION_V2_FILTER_H
#define HYPERION_V2_FILTER_H

#include "render_pass.h"
#include "framebuffer.h"
#include "shader.h"

#include <rendering/backend/renderer_frame_handler.h>

#include <memory>

namespace hyperion::renderer {

class Instance;
class Frame;

} // namespace hyperion::renderer

/* forward declaration of mesh class */

namespace hyperion {
class Mesh;
} // namespace hyperion

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Pipeline;
using renderer::CommandBuffer;
using renderer::PerFrameData;
using renderer::MeshInputAttributeSet;

class Filter {
public:
    static const MeshInputAttributeSet vertex_attributes;
    static const std::shared_ptr<Mesh> full_screen_quad;

    Filter(Shader::ID shader_id);
    Filter(const Filter &) = delete;
    Filter &operator=(const Filter &) = delete;
    ~Filter();

    inline bool IsRecorded() const
        { return m_recorded; }

    inline auto &GetFrameData()
        { return m_frame_data; }
    inline const auto &GetFrameData() const
        { return m_frame_data; }

    void CreateRenderPass(Engine *engine);
    void CreateFrameData(Engine *engine);
    void CreateDescriptors(Engine *engine, uint32_t &binding_offset);
    void CreatePipeline(Engine *engine);

    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame, uint32_t frame_index);
    void Record(Engine *engine, uint32_t frame_index);

private:
    std::unique_ptr<PerFrameData<CommandBuffer>> m_frame_data;
    Framebuffer::ID m_framebuffer_id;
    Shader::ID m_shader_id;
    RenderPass::ID m_render_pass_id;
    Pipeline *m_pipeline;
    bool m_recorded;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_FILTER_STACK_H

