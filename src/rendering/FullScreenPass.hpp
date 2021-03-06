#ifndef HYPERION_V2_FULL_SCREEN_PASS_H
#define HYPERION_V2_FULL_SCREEN_PASS_H

#include "RenderPass.hpp"
#include "Framebuffer.hpp"
#include "Shader.hpp"
#include "Renderer.hpp"
#include "Mesh.hpp"
#include <Constants.hpp>

#include <Types.hpp>
#include <core/Containers.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererImage.hpp>

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
    
    FullScreenPass(
        Image::InternalFormat image_format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8_SRGB
    );
    FullScreenPass(
        Ref<Shader> &&shader,
        Image::InternalFormat image_format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8_SRGB
    );
    FullScreenPass(
        Ref<Shader> &&shader,
        DescriptorKey descriptor_key,
        UInt sub_descriptor_index,
        Image::InternalFormat image_format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8_SRGB
    );
    FullScreenPass(const FullScreenPass &) = delete;
    FullScreenPass &operator=(const FullScreenPass &) = delete;
    ~FullScreenPass();
    
    CommandBuffer *GetCommandBuffer(UInt index) const { return m_command_buffers[index].get(); }
    Framebuffer *GetFramebuffer(UInt index) const     { return m_framebuffers[index].ptr; }
                                                      
    Shader *GetShader() const                         { return m_shader.ptr; }
    void SetShader(Ref<Shader> &&shader);             
                                                      
    RenderPass *GetRenderPass() const                 { return m_render_pass.ptr; }
                                                      
    RendererInstance *GetRendererInstance() const     { return m_renderer_instance.ptr; }
                                                      
    UInt GetSubDescriptorIndex() const                { return m_sub_descriptor_index; }

    PushConstantData &GetPushConstants()              { return m_push_constant_data; }
    const PushConstantData &GetPushConstants() const  { return m_push_constant_data; }
    void SetPushConstants(const PushConstantData &pc) { m_push_constant_data = pc; }

    void CreateRenderPass(Engine *engine);
    void Create(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void CreatePipeline(Engine *engine, const RenderableAttributeSet &renderable_attributes);
    void CreatePipeline(Engine *engine);

    void Destroy(Engine *engine);

    void Render(Engine *engine, Frame *frame);
    void Record(Engine *engine, UInt frame_index);

protected:
    FixedArray<std::unique_ptr<CommandBuffer>, max_frames_in_flight> m_command_buffers;
    FixedArray<Ref<Framebuffer>, max_frames_in_flight>               m_framebuffers;
    Ref<Shader>                                                      m_shader;
    Ref<RenderPass>                                                  m_render_pass;
    Ref<RendererInstance>                                            m_renderer_instance;
                                                                     
    DynArray<std::unique_ptr<Attachment>>                            m_attachments;

    PushConstantData                                                 m_push_constant_data;

private:                         
    Image::InternalFormat                                            m_image_format;                                    
    DescriptorKey                                                    m_descriptor_key;
    UInt                                                             m_sub_descriptor_index;
};
} // namespace hyperion::v2

#endif