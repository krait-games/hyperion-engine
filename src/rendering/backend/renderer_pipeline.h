//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_PIPELINE_H
#define HYPERION_RENDERER_PIPELINE_H

#include <vulkan/vulkan.h>

#include "renderer_device.h"
#include "renderer_swapchain.h"
#include "renderer_buffer.h"
#include "renderer_shader.h"
#include "renderer_render_pass.h"
#include "renderer_descriptor_pool.h"
#include "renderer_descriptor_set.h"
#include "renderer_descriptor.h"
#include "renderer_helpers.h"

#include "../../hash_code.h"

namespace hyperion {
namespace renderer {
class FramebufferObject;
class Pipeline {
public:
    struct ConstructionInfo {
        MeshInputAttributeSet vertex_attributes;
        non_owning_ptr<Shader> shader;
        int shader_id;
        non_owning_ptr<RenderPass> render_pass;
        int render_pass_id;
        std::vector<non_owning_ptr<FramebufferObject>> fbos;
        std::vector<int> fbo_ids; // unresolved fbo ids

        VkPrimitiveTopology topology;

        enum class CullMode : int {
            NONE,
            BACK,
            FRONT
        } cull_mode;

        bool depth_test,
             depth_write;

        ConstructionInfo() : shader_id(-1), render_pass_id(-1) {}
        ConstructionInfo(ConstructionInfo &&other)
            : vertex_attributes(std::move(other.vertex_attributes)),
              shader(other.shader),
              shader_id(other.shader_id),
              render_pass(other.render_pass),
              render_pass_id(other.render_pass_id),
              fbos(std::move(other.fbos)),
              fbo_ids(std::move(other.fbo_ids)),
              topology(other.topology),
              cull_mode(other.cull_mode),
              depth_test(other.depth_test),
              depth_write(other.depth_write)
        {
        }

        ConstructionInfo &operator=(ConstructionInfo &&other)
        {
            vertex_attributes = std::move(other.vertex_attributes);
            shader = other.shader;
            shader_id = other.shader_id;
            render_pass = other.render_pass;
            render_pass_id = other.render_pass_id;
            fbos = std::move(other.fbos);
            fbo_ids = std::move(other.fbo_ids);
            topology = other.topology;
            cull_mode = other.cull_mode;
            depth_test = other.depth_test;
            depth_write = other.depth_write;

            return *this;
        }

        ConstructionInfo(const ConstructionInfo &other) = delete;
        ConstructionInfo &operator=(const ConstructionInfo &other) = delete;
        ~ConstructionInfo() = default;

        inline HashCode GetHashCode() const
        {
            HashCode hc;

            hc.Add(shader_id);
            hc.Add(render_pass_id); // TODO
            for (auto fbo_id : fbo_ids) {
                hc.Add(fbo_id);
            }
            hc.Add(vertex_attributes.GetHashCode());
            hc.Add(int(topology));
            hc.Add(int(cull_mode));
            hc.Add(depth_test);
            hc.Add(depth_write);

            return hc;
        }
    };

    class Builder {
    public:
        Builder()
        {
            Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            CullMode(Pipeline::ConstructionInfo::CullMode::BACK);
            DepthTest(true);
            DepthWrite(true);
        }

        Builder(Builder &&other) = default;
        Builder(const Builder &other) = delete;
        Builder &operator=(const Builder &other) = delete;
        ~Builder() = default;

        Builder &VertexAttributes(const MeshInputAttributeSet &vertex_attributes)
        {
            m_construction_info.vertex_attributes = vertex_attributes;

            return *this;
        }

        Builder &Topology(VkPrimitiveTopology topology)
        {
            m_construction_info.topology = topology;

            return *this;
        }

        Builder &CullMode(Pipeline::ConstructionInfo::CullMode cull_mode)
        {
            m_construction_info.cull_mode = cull_mode;

            return *this;
        }

        Builder &DepthWrite(bool depth_write)
        {
            m_construction_info.depth_write = depth_write;

            return *this;
        }

        Builder &DepthTest(bool depth_test)
        {
            m_construction_info.depth_test = depth_test;

            return *this;
        }

        template <class T>
        Builder &Shader(typename T::ID id)
        {
            m_construction_info.shader_id = id.GetValue();

            return *this;
        }

        template <class T>
        Builder &RenderPass(typename T::ID id)
        {
            m_construction_info.render_pass_id = id.GetValue();

            return *this;
        }

        template <class T>
        Builder &Framebuffer(typename T::ID id)
        {
            m_construction_info.fbo_ids.push_back(id.GetValue());

            return *this;
        }

        std::unique_ptr<Pipeline> Build(Device *device)
        {
            AssertThrow(!m_construction_info.fbos.empty());

            return std::make_unique<Pipeline>(device, std::move(m_construction_info));
        }

        inline HashCode GetHashCode() const
        {
            return m_construction_info.GetHashCode();
        }
        
        ConstructionInfo m_construction_info;
    };

    Pipeline(Device *_device, ConstructionInfo &&construction_info);
    void Destroy();

    std::vector<VkDynamicState> GetDynamicStates();
    void SetDynamicStates(const std::vector<VkDynamicState> &_states);

    void UpdateDynamicStates(VkCommandBuffer cmd);
    void SetViewport(float x, float y, float width, float height, float min_depth = 0.0f, float max_depth = 1.0f);
    void SetScissor(int x, int y, uint32_t width, uint32_t height);
    void SetVertexInputMode(std::vector<VkVertexInputBindingDescription> &binding_descs, std::vector<VkVertexInputAttributeDescription> &vertex_attribs);

    inline void Build(DescriptorPool *descriptor_pool)
    {
        Rebuild(descriptor_pool);
    }

    inline void Build(ConstructionInfo &&construction_info, DescriptorPool *descriptor_pool)
    {
        m_construction_info = std::move(construction_info);

        Build(descriptor_pool);
    }

    void BeginRenderPass(VkCommandBuffer cmd, size_t index, VkSubpassContents contents);
    void EndRenderPass(VkCommandBuffer cmd, size_t index);
    void Bind(VkCommandBuffer cmd);

    inline const ConstructionInfo &GetConstructionInfo() const { return m_construction_info; }

    VkPipeline pipeline;
    VkPipelineLayout layout;

    struct PushConstants {
        uint32_t previous_frame_index;
        uint32_t current_frame_index;
    } push_constants;

private:
    void Rebuild(DescriptorPool *descriptor_pool);

    std::vector<VkDynamicState> dynamic_states;

    VkViewport viewport;
    VkRect2D scissor;

    std::vector<VkVertexInputBindingDescription>   vertex_binding_descriptions = { };
    std::vector<VkVertexInputAttributeDescription> vertex_attributes = { };

    Device *device;

    ConstructionInfo m_construction_info;

    std::vector<VkVertexInputAttributeDescription> BuildVertexAttributes(const MeshInputAttributeSet &attribute_set);
};

} // namespace renderer
}; // namespace hyperion

#endif //HYPERION_RENDERER_PIPELINE_H
