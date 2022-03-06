#ifndef HYPERION_RENDERER_RENDER_PASS_H
#define HYPERION_RENDERER_RENDER_PASS_H

#include "renderer_image.h"
#include "renderer_image_view.h"
#include "renderer_sampler.h"
#include "renderer_attachment.h"

#include <vulkan/vulkan.h>

namespace hyperion {

class RendererRenderPass {
    friend class RendererFramebufferObject;
    friend class RendererPipeline;
public:
    struct AttachmentInfo {
        std::unique_ptr<RendererAttachment> attachment;
        bool is_depth_attachment;
    };

    RendererRenderPass();
    RendererRenderPass(const RendererRenderPass &other) = delete;
    RendererRenderPass &operator=(const RendererRenderPass &other) = delete;
    ~RendererRenderPass();

    void AddAttachment(AttachmentInfo &&attachment);
    inline void AddDependency(const VkSubpassDependency &dependency)
        { m_dependencies.push_back(dependency); }

    inline std::vector<AttachmentInfo> &GetAttachmentInfos() { return m_attachment_infos; }
    inline const std::vector<AttachmentInfo> &GetAttachmentInfos() const { return m_attachment_infos; }

    inline VkRenderPass GetRenderPass() const { return m_render_pass; }

    RendererResult Create(RendererDevice *device);
    RendererResult Destroy(RendererDevice *device);

    void Begin(VkCommandBuffer cmd, VkFramebuffer framebuffer, VkExtent2D extent);
    void End(VkCommandBuffer cmd);

private:

    std::vector<AttachmentInfo> m_attachment_infos;
    std::vector<VkSubpassDependency> m_dependencies;
    VkRenderPass m_render_pass;
};

} // namespace hyperion

#endif