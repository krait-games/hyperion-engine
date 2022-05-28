#ifndef HYPERION_RENDERER_FRAME_HANDLER_H
#define HYPERION_RENDERER_FRAME_HANDLER_H

#include "renderer_frame.h"
#include "renderer_swapchain.h"
#include "renderer_command_buffer.h"
#include "renderer_structs.h"
#include "renderer_queue.h"

namespace hyperion {
namespace renderer {

class FrameHandler {
public:
    using NextImageFunction = std::add_pointer_t<VkResult(Device *device, Swapchain *swapchain, Frame *frame, uint32_t *image_index)>;

    FrameHandler(uint32_t num_frames, NextImageFunction next_image);
    FrameHandler(const FrameHandler &other) = delete;
    FrameHandler &operator=(const FrameHandler &other) = delete;
    ~FrameHandler();

    inline auto &GetPerFrameData() { return m_per_frame_data; }
    inline const auto &GetPerFrameData() const { return m_per_frame_data; }

    inline auto &GetCurrentFrameData() { return m_per_frame_data[m_current_frame_index]; }
    inline const auto &GetCurrentFrameData() const { return m_per_frame_data[m_current_frame_index]; }

    inline uint32_t NumFrames() const { return m_per_frame_data.NumFrames(); }
    inline uint32_t GetAcquiredImageIndex() const { return m_acquired_image_index; }
    inline uint32_t GetCurrentFrameIndex() const { return m_current_frame_index; }

    /* Used to acquire a new image from the provided next_image function.
     * Really only useful for our main swapchain surface */
    Result PrepareFrame(Device *device, Swapchain *swapchain);
    /* Submit the frame for presentation */
    Result PresentFrame(Queue *queue, Swapchain *swapchain) const;
    /* Advance the current frame index; call at the end of a render loop. */
    void NextFrame();
    /* Create our Frame objects (count is same as num_frames) */
    Result CreateFrames(Device *device);
    /* Create our CommandBuffer objects (count is same as num_frames) */
    Result CreateCommandBuffers(Device *device, VkCommandPool pool);
    Result Destroy(Device *device, VkCommandPool pool);

private:
    PerFrameData<CommandBuffer, Frame> m_per_frame_data;

    NextImageFunction m_next_image;
    uint32_t m_acquired_image_index;
    uint32_t m_current_frame_index;
};

} // namespace renderer
} // namespace hyperion

#endif