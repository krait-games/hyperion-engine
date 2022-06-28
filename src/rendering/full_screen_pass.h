#ifndef HYPERION_V2_FULL_SCREEN_PASS_H
#define HYPERION_V2_FULL_SCREEN_PASS_H

#include "render_pass.h"
#include "framebuffer.h"
#include "shader.h"
#include "graphics.h"
#include "mesh.h"
#include <constants.h>

#include <types.h>

#include <rendering/backend/renderer_frame.h>
#include <rendering/backend/renderer_structs.h>
#include <rendering/backend/renderer_command_buffer.h>
#include <rendering/backend/renderer_image.h>

#include <memory>
#include <utility>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::CommandBuffer;
using renderer::PerFrameData;
using renderer::VertexAttributeSet;
using renderer::DescriptorKey;
using renderer::Image;
using renderer::Pipeline;

class Engine;

class FullScreenPass {
    using PushConstantData = Pipeline::PushConstantData;

public:
    static std::unique_ptr<Mesh> full_screen_quad;
    
    FullScreenPass();
    FullScreenPass(Ref<Shader> &&shader);
    FullScreenPass(
        Ref<Shader> &&shader,
        DescriptorKey descriptor_key,
        UInt sub_descriptor_index
    );
    FullScreenPass(const FullScreenPass &) = delete;
    FullScreenPass &operator=(const FullScreenPass &) = delete;
    ~FullScreenPass();

    
    CommandBuffer *GetCommandBuffer(UInt index) const { return m_command_buffers[index].get(); }
    Framebuffer *GetFramebuffer(UInt index) const     { return m_framebuffers[index].ptr; }
                                                      
    Shader *GetShader() const                         { return m_shader.ptr; }
    void SetShader(Ref<Shader> &&shader);             
                                                      
    RenderPass *GetRenderPass() const                 { return m_render_pass.ptr; }
                                                      
    GraphicsPipeline *GetGraphicsPipeline() const     { return m_pipeline.ptr; }
                                                      
    UInt GetSubDescriptorIndex() const                { return m_sub_descriptor_index; }

    PushConstantData &GetPushConstants()              { return m_push_constant_data; }
    const PushConstantData &GetPushConstants() const  { return m_push_constant_data; }
    void SetPushConstants(const PushConstantData &pc) { m_push_constant_data = pc; }

    void CreateRenderPass(Engine *engine);
    void Create(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void CreatePipeline(Engine *engine);

    void Destroy(Engine *engine);

    void Render(Engine *engine, Frame *frame);
    void Record(Engine *engine, UInt frame_index);

protected:
    std::array<std::unique_ptr<CommandBuffer>, max_frames_in_flight> m_command_buffers;
    std::array<Ref<Framebuffer>, max_frames_in_flight>               m_framebuffers;
    Ref<Shader>                                                      m_shader;
    Ref<RenderPass>                                                  m_render_pass;
    Ref<GraphicsPipeline>                                            m_pipeline;
                                                                     
    std::vector<std::unique_ptr<Attachment>>                         m_attachments;

    PushConstantData                                                 m_push_constant_data;

private:                                                             
    DescriptorKey                                                    m_descriptor_key;
    UInt                                                             m_sub_descriptor_index;
};
} // namespace hyperion::v2

#endif