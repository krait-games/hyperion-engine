//
// Created by emd22 on 2022-02-22.
//

#ifndef HYPERION_RENDERER_BUFFER_H
#define HYPERION_RENDERER_BUFFER_H

#include "renderer_result.h"
#include "renderer_structs.h"
#include "../../system/vma/vma_usage.h"

namespace hyperion {
namespace renderer {

class Device;
class CommandBuffer;

class GPUMemory {
public:
    static uint32_t FindMemoryType(Device *device, uint32_t vk_type_filter, VkMemoryPropertyFlags properties);

    GPUMemory(
        uint32_t memory_property_flags,
        uint32_t sharing_mode
    );
    GPUMemory(const GPUMemory &other) = delete;
    GPUMemory &operator=(const GPUMemory &other) = delete;
    ~GPUMemory();
    
    VmaAllocation allocation;
    VkDeviceSize size;

    void Map(Device *device, void **ptr);
    void Unmap(Device *device);
    void Copy(Device *device, size_t count, void *ptr);
    void Copy(Device *device, size_t offset, size_t count, void *ptr);

protected:
    uint32_t sharing_mode;
    uint32_t memory_property_flags;
};

/* buffers */

class GPUBuffer : public GPUMemory {
public:
    GPUBuffer(
        VkBufferUsageFlags usage_flags,
        uint32_t memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uint32_t sharing_mode = VK_SHARING_MODE_EXCLUSIVE
    );
    GPUBuffer(const GPUBuffer &other) = delete;
    GPUBuffer &operator=(const GPUBuffer &other) = delete;
    ~GPUBuffer();

    void Create(Device *device, size_t buffer_size);
    void Destroy(Device *device);

    VkBuffer buffer;

private:
    VkBufferUsageFlags usage_flags;
};

class VertexBuffer : public GPUBuffer {
public:
    VertexBuffer(
        uint32_t memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uint32_t sharing_mode = VK_SHARING_MODE_EXCLUSIVE
    );

    void Bind(CommandBuffer *command_buffer);
};

class IndexBuffer : public GPUBuffer {
public:
    IndexBuffer(
        uint32_t memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uint32_t sharing_mode = VK_SHARING_MODE_EXCLUSIVE
    );

    void Bind(CommandBuffer *command_buffer);

    inline DatumType GetDatumType() const { return m_datum_type; }
    inline void SetDatumType(DatumType datum_type) { m_datum_type = datum_type; }

private:
    DatumType m_datum_type = DatumType::UNSIGNED_INT;
};

class UniformBuffer : public GPUBuffer {
public:
    UniformBuffer(
        uint32_t memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uint32_t sharing_mode = VK_SHARING_MODE_EXCLUSIVE
    );
};

class StorageBuffer : public GPUBuffer {
public:
    StorageBuffer(
        uint32_t memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uint32_t sharing_mode = VK_SHARING_MODE_EXCLUSIVE
    );
};

class StagingBuffer : public GPUBuffer {
public:
    StagingBuffer(
        uint32_t memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uint32_t sharing_mode = VK_SHARING_MODE_EXCLUSIVE
    );
};

/* images */


class GPUImage : public GPUMemory {
public:
    GPUImage(
        VkImageUsageFlags usage_flags,
        uint32_t memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        uint32_t sharing_mode = VK_SHARING_MODE_EXCLUSIVE
    );
    GPUImage(const GPUImage &other) = delete;
    GPUImage &operator=(const GPUImage &other) = delete;
    ~GPUImage();

    Result Create(Device *device, size_t size, VkImageCreateInfo *image_info);
    Result Destroy(Device *device);

    VkImage image;

private:
    VkImageUsageFlags usage_flags;
};

} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BUFFER_H
