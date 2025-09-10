#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <string>

/**
 * VulkanMon Resource Management System
 * 
 * Provides RAII wrappers for Vulkan resources following our core philosophy:
 * - "Simple is Powerful" - Clean, obvious resource lifetime management
 * - "Test, Test, Test" - Safe constructors, destructors, and error handling
 * - "Document Often" - Clear intent and usage patterns
 * 
 * Key Features:
 * - RAII lifetime management for all Vulkan objects
 * - Move-only semantics to prevent resource duplication
 * - Debug naming support for development tools
 * - Automatic cleanup ordering
 */

namespace VulkanMon {

/**
 * Base class for all managed Vulkan resources
 * Provides common functionality for debug naming and device tracking
 */
class ManagedResource {
public:
    explicit ManagedResource(VkDevice device, const std::string& debugName = "")
        : device_(device), debugName_(debugName) {}
    
    virtual ~ManagedResource() = default;
    
    // Move-only semantics
    ManagedResource(const ManagedResource&) = delete;
    ManagedResource& operator=(const ManagedResource&) = delete;
    ManagedResource(ManagedResource&&) = default;
    ManagedResource& operator=(ManagedResource&&) = default;
    
    const std::string& getDebugName() const { return debugName_; }
    VkDevice getDevice() const { return device_; }

protected:
    VkDevice device_;
    std::string debugName_;
};

/**
 * RAII wrapper for VkBuffer + VkDeviceMemory
 * Automatically handles creation and cleanup of buffer resources
 * Note: Should be created through ResourceManager for proper memory type handling
 */
class ManagedBuffer : public ManagedResource {
public:
    ManagedBuffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, 
                  uint32_t memoryTypeIndex, const std::string& debugName = "");
    ~ManagedBuffer();
    
    // Move constructor and assignment
    ManagedBuffer(ManagedBuffer&& other) noexcept;
    ManagedBuffer& operator=(ManagedBuffer&& other) noexcept;
    
    VkBuffer getBuffer() const { return buffer_; }
    VkDeviceMemory getMemory() const { return memory_; }
    VkDeviceSize getSize() const { return size_; }
    
    // Memory mapping helpers
    void* map();
    void unmap();
    void updateData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

private:
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkDeviceSize size_ = 0;
    void* mappedData_ = nullptr;
    
    void cleanup();
};

/**
 * RAII wrapper for VkImage + VkDeviceMemory + VkImageView
 * Handles complete image resource lifecycle
 * Note: Should be created through ResourceManager for proper memory type handling
 */
class ManagedImage : public ManagedResource {
public:
    ManagedImage(VkDevice device, uint32_t width, uint32_t height, VkFormat format,
                 VkImageTiling tiling, VkImageUsageFlags usage, uint32_t memoryTypeIndex,
                 VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                 const std::string& debugName = "");
    ~ManagedImage();
    
    // Move constructor and assignment  
    ManagedImage(ManagedImage&& other) noexcept;
    ManagedImage& operator=(ManagedImage&& other) noexcept;
    
    VkImage getImage() const { return image_; }
    VkDeviceMemory getMemory() const { return memory_; }
    VkImageView getImageView() const { return imageView_; }
    VkFormat getFormat() const { return format_; }
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }

private:
    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkImageView imageView_ = VK_NULL_HANDLE;
    VkFormat format_;
    uint32_t width_, height_;
    
    void cleanup();
};

/**
 * Resource Manager - Central registry for all engine resources
 * Provides resource creation, tracking, and cleanup coordination
 */
class ResourceManager {
public:
    explicit ResourceManager(VkDevice device, VkPhysicalDevice physicalDevice);
    ~ResourceManager();
    
    // Move-only semantics
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = default;
    ResourceManager& operator=(ResourceManager&&) = default;
    
    // Resource creation methods
    std::unique_ptr<ManagedBuffer> createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                                VkMemoryPropertyFlags properties, 
                                                const std::string& debugName = "");
    
    std::unique_ptr<ManagedImage> createImage(uint32_t width, uint32_t height, VkFormat format,
                                              VkImageTiling tiling, VkImageUsageFlags usage,
                                              VkMemoryPropertyFlags properties,
                                              VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                                              const std::string& debugName = "");
    
    // Helper methods for memory type finding
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    // Statistics and debugging
    size_t getBufferCount() const { return bufferCount_; }
    size_t getImageCount() const { return imageCount_; }
    VkDeviceSize getTotalAllocatedMemory() const { return totalAllocatedMemory_; }
    
    void printResourceSummary() const;

private:
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    
    // Resource tracking
    size_t bufferCount_ = 0;
    size_t imageCount_ = 0;
    VkDeviceSize totalAllocatedMemory_ = 0;
};

} // namespace VulkanMon