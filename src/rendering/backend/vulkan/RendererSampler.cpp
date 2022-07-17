#include "RendererSampler.hpp"
#include "RendererDevice.hpp"
#include "RendererFeatures.hpp"

#include <system/Debug.hpp>

namespace hyperion {
namespace renderer {
Sampler::Sampler(Image::FilterMode filter_mode, Image::WrapMode wrap_mode)
    : m_sampler(VK_NULL_HANDLE),
      m_filter_mode(filter_mode),
      m_wrap_mode(wrap_mode)
{
}

Sampler::~Sampler()
{
    AssertThrowMsg(m_sampler == VK_NULL_HANDLE, "sampler should have been destroyed");
}

Result Sampler::Create(Device *device)
{
    VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_info.magFilter    = Image::ToVkFilter(m_filter_mode);
    sampler_info.minFilter    = Image::ToVkFilter(m_filter_mode);

    sampler_info.addressModeU = Image::ToVkSamplerAddressMode(m_wrap_mode);
    sampler_info.addressModeV = Image::ToVkSamplerAddressMode(m_wrap_mode);
    sampler_info.addressModeW = Image::ToVkSamplerAddressMode(m_wrap_mode);

    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy    = device->GetFeatures().GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy;

    sampler_info.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable           = VK_FALSE;
    sampler_info.compareOp               = VK_COMPARE_OP_ALWAYS;
    
    switch (m_filter_mode) {
    case Image::FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP:
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    case Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP:
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        break;
    case Image::FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP:
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    default:
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    }

    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod     = 0.0f;
    sampler_info.maxLod     = 12.0f;

    VkSamplerReductionModeCreateInfoEXT reduction_info{VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT};

    if (m_filter_mode == Image::FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP) {
        if (!device->GetFeatures().GetSamplerMinMaxProperties().filterMinmaxSingleComponentFormats) {
            return { Result::RENDERER_ERR, "Device does not support min/max sampler formats" };
        }

        reduction_info.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN;
        sampler_info.pNext           = &reduction_info;
    }

    if (vkCreateSampler(device->GetDevice(), &sampler_info, nullptr, &m_sampler) != VK_SUCCESS) {
        return { Result::RENDERER_ERR, "Failed to create sampler!" };
    }

    HYPERION_RETURN_OK;
}

Result Sampler::Destroy(Device *device)
{
    vkDestroySampler(device->GetDevice(), m_sampler, nullptr);

    m_sampler = VK_NULL_HANDLE;

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion
