#include "renderer_descriptor_set.h"
#include "renderer_device.h"
#include "../../system/debug.h"

namespace hyperion {
namespace renderer {
Descriptor::Descriptor(uint32_t binding, const BufferInfo &info, VkDescriptorType type, VkShaderStageFlags stage_flags)
    : m_binding(binding),
      m_info(info),
      m_type(type),
      m_stage_flags(stage_flags)
{
}

Descriptor::~Descriptor()
{
}

void Descriptor::Create(Device *device, Descriptor::Info *out_info)
{
    AssertExit(m_info.mode != Mode::UNSET);

    switch (m_info.mode) {
    case Mode::BUFFER:
        AssertExit(m_info.gpu_buffer != nullptr);
        AssertExit(m_info.gpu_buffer->buffer != nullptr);

        m_info.buffer_info.buffer = m_info.gpu_buffer->buffer;
        m_info.buffer_info.offset = 0; // TODO: re-use of buffers and offsetting of data
        m_info.buffer_info.range = m_info.gpu_buffer->size;

        break;
    case Mode::IMAGE:
        AssertExit(m_info.image_view != nullptr);
        AssertExit(m_info.sampler != nullptr);

        m_info.image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_info.image_info.imageView = m_info.image_view->GetImageView();
        m_info.image_info.sampler = m_info.sampler->GetSampler();

        break;
    }

    out_info->binding = VkDescriptorSetLayoutBinding{};
    out_info->binding.descriptorCount = 1;
    out_info->binding.descriptorType = m_type;
    out_info->binding.pImmutableSamplers = nullptr;
    out_info->binding.stageFlags = m_stage_flags;
    out_info->binding.binding = m_binding;

    out_info->write = VkWriteDescriptorSet{};
    out_info->write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    out_info->write.pNext = nullptr;
    out_info->write.descriptorCount = 1;
    out_info->write.descriptorType = m_type;
    out_info->write.pBufferInfo = &m_info.buffer_info;
    out_info->write.pImageInfo = &m_info.image_info;
    out_info->write.dstBinding = m_binding;
}

void Descriptor::Destroy(Device *device)
{
}

} // namespace renderer
} // namespace hyperion