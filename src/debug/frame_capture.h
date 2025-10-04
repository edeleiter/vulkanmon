#pragma once
#include <vulkan/vulkan.h>
#include <string>

namespace VulkanMon::Debug {

class FrameCapture {
public:
    /**
     * Capture Vulkan image to PNG file
     * @param device Vulkan logical device
     * @param physicalDevice Vulkan physical device
     * @param commandPool Command pool for transfer commands
     * @param queue Graphics queue for command submission
     * @param image Source image to capture
     * @param format Image format
     * @param width Image width
     * @param height Image height
     * @param filename Output PNG file path
     * @return true if capture successful
     */
    static bool CaptureToFile(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkCommandPool commandPool,
        VkQueue queue,
        VkImage image,
        VkFormat format,
        uint32_t width,
        uint32_t height,
        const std::string& filename
    );

private:
    static bool SaveImageToPNG(
        const void* data,
        uint32_t width,
        uint32_t height,
        uint32_t channels,
        const std::string& filename
    );
};

} // namespace VulkanMon::Debug
