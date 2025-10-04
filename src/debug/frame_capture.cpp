#include "frame_capture.h"
#include "../utils/Logger.h"
#include <vector>
#include <cstring>

// TODO: Add stb_image_write implementation when ready for frame capture
// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include <stb_image_write.h>

namespace VulkanMon::Debug {

bool FrameCapture::CaptureToFile(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue queue,
    VkImage image,
    VkFormat format,
    uint32_t width,
    uint32_t height,
    const std::string& filename
) {
    // TODO: Implement Vulkan framebuffer capture
    // 1. Create staging buffer
    // 2. Transition image layout to TRANSFER_SRC_OPTIMAL
    // 3. vkCmdCopyImageToBuffer
    // 4. Transition image layout back
    // 5. Wait for copy to complete
    // 6. Map staging buffer memory
    // 7. Convert format if needed (BGR -> RGB)
    // 8. Call SaveImageToPNG
    // 9. Unmap and cleanup

    Logger::getInstance().warning("[FrameCapture] Not yet implemented");
    return false;
}

bool FrameCapture::SaveImageToPNG(
    const void* data,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    const std::string& filename
) {
    // TODO: Implement when stb_image_write is available
    // int result = stbi_write_png(
    //     filename.c_str(),
    //     width,
    //     height,
    //     channels,
    //     data,
    //     width * channels
    // );

    Logger::getInstance().info("[FrameCapture] PNG save to: " + filename + " (not yet implemented)");
    return false;
}

} // namespace VulkanMon::Debug
