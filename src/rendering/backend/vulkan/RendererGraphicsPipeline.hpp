#ifndef HYPERION_RENDERER_GRAPHICS_PIPELINE_H
#define HYPERION_RENDERER_GRAPHICS_PIPELINE_H

#include <vulkan/vulkan.h>

#include "RendererPipeline.hpp"
#include "RendererDevice.hpp"
#include "RendererSwapchain.hpp"
#include "RendererBuffer.hpp"
#include "RendererShader.hpp"
#include "RendererRenderPass.hpp"
#include "RendererDescriptorSet.hpp"
#include "RendererCommandBuffer.hpp"
#include "RendererStructs.hpp"
#include "RendererHelpers.hpp"

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {

class FramebufferObject;

class GraphicsPipeline : public Pipeline {
public:
    struct ConstructionInfo {
        VertexAttributeSet vertex_attributes;

        Topology topology       = Topology::TRIANGLES;
        FaceCullMode cull_mode  = FaceCullMode::BACK;
        FillMode fill_mode      = FillMode::FILL;

        bool depth_test         = true,
             depth_write        = true,
             blend_enabled      = false;

        ShaderProgram *shader   = nullptr;
        RenderPass *render_pass = nullptr;
        std::vector<FramebufferObject *> fbos;

        // stencil
        StencilState stencil_state{};

        uint32_t    multiview_index = ~0u;
    };

    GraphicsPipeline();
    GraphicsPipeline(const GraphicsPipeline &other) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &other) = delete;
    ~GraphicsPipeline();

    const std::vector<VkDynamicState> &GetDynamicStates() const
        { return dynamic_states; }

    void SetDynamicStates(const std::vector<VkDynamicState> &states)
        { dynamic_states = states; }

    void SetViewport(float x, float y, float width, float height, float min_depth = 0.0f, float max_depth = 1.0f);
    void SetScissor(int x, int y, uint32_t width, uint32_t height);
    void SetVertexInputMode(std::vector<VkVertexInputBindingDescription> &binding_descs, std::vector<VkVertexInputAttributeDescription> &vertex_attribs);
    
    Result Create(Device *device, ConstructionInfo &&construction_info, DescriptorPool *descriptor_pool);
    Result Destroy(Device *device);
    
    void Bind(CommandBuffer *cmd);
    void SubmitPushConstants(CommandBuffer *cmd) const;

    const ConstructionInfo &GetConstructionInfo() const { return m_construction_info; }

private:
    Result Rebuild(Device *device, DescriptorPool *descriptor_pool);
    void UpdateDynamicStates(VkCommandBuffer cmd);
    std::vector<VkVertexInputAttributeDescription> BuildVertexAttributes(const VertexAttributeSet &attribute_set);

    std::vector<VkDynamicState> dynamic_states;

    VkViewport viewport;
    VkRect2D scissor;

    std::vector<VkVertexInputBindingDescription>   vertex_binding_descriptions{};
    std::vector<VkVertexInputAttributeDescription> vertex_attributes{};

    ConstructionInfo m_construction_info;

};

} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_GRAPHICS_PIPELINE_H
