#include "frame_capture.h"
#include "../utils/Logger.h"
#include <vector>
#include <cstring>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

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
    Logger::getInstance().info("[FrameCapture] Starting capture: " + std::to_string(width) + "x" + std::to_string(height));

    // 1. Create staging buffer for CPU access
    VkDeviceSize imageSize = width * height * 4; // RGBA format

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    // Create staging buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        Logger::getInstance().error("[FrameCapture] Failed to create staging buffer");
        return false;
    }

    // Allocate staging buffer memory
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    uint32_t memoryTypeIndex = 0;
    bool foundMemoryType = false;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            foundMemoryType = true;
            break;
        }
    }

    if (!foundMemoryType) {
        Logger::getInstance().error("[FrameCapture] Failed to find suitable memory type");
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory) != VK_SUCCESS) {
        Logger::getInstance().error("[FrameCapture] Failed to allocate staging buffer memory");
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return false;
    }

    vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);

    // 2. Create command buffer for copy operation
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);

    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // 3. Transition image layout to TRANSFER_SRC_OPTIMAL
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // Image is in present layout after rendering
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    // 4. Copy image to buffer
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyImageToBuffer(
        commandBuffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        stagingBuffer,
        1,
        &region
    );

    // 5. Transition image layout back to PRESENT_SRC_KHR
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    vkEndCommandBuffer(commandBuffer);

    // 6. Submit command buffer and wait for completion
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    // 7. Map staging buffer memory
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);

    // 8. Convert format if needed (BGR -> RGB for some formats)
    std::vector<uint8_t> convertedData;
    bool needsConversion = (format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB);

    if (needsConversion) {
        convertedData.resize(imageSize);
        uint8_t* src = static_cast<uint8_t*>(data);
        uint8_t* dst = convertedData.data();

        for (size_t i = 0; i < imageSize; i += 4) {
            dst[i + 0] = src[i + 2]; // R = B
            dst[i + 1] = src[i + 1]; // G = G
            dst[i + 2] = src[i + 0]; // B = R
            dst[i + 3] = src[i + 3]; // A = A
        }

        data = convertedData.data();
    }

    // 9. Save to PNG
    bool success = SaveImageToPNG(data, width, height, 4, filename);

    // 10. Cleanup
    vkUnmapMemory(device, stagingBufferMemory);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    vkDestroyBuffer(device, stagingBuffer, nullptr);

    if (success) {
        Logger::getInstance().info("[FrameCapture] Successfully saved to: " + filename);
    } else {
        Logger::getInstance().error("[FrameCapture] Failed to save PNG");
    }

    return success;
}

bool FrameCapture::SaveImageToPNG(
    const void* data,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    const std::string& filename
) {
    int result = stbi_write_png(
        filename.c_str(),
        width,
        height,
        channels,
        data,
        width * channels
    );

    return result != 0;
}

} // namespace VulkanMon::Debug
