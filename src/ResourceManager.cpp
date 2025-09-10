#include "ResourceManager.h"
#include <stdexcept>
#include <iostream>

namespace VulkanMon {

// ============================================================================
// ManagedBuffer Implementation
// ============================================================================

ManagedBuffer::ManagedBuffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage,
                             uint32_t memoryTypeIndex, const std::string& debugName)
    : ManagedResource(device, debugName), size_(size) {
    
    // Create the buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer: " + debugName);
    }
    
    // Get memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, buffer_, &memRequirements);
    
    // Allocate memory using provided memory type index
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory_) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buffer_, nullptr);
        throw std::runtime_error("Failed to allocate buffer memory: " + debugName);
    }
    
    // Bind buffer to memory
    vkBindBufferMemory(device_, buffer_, memory_, 0);
}

ManagedBuffer::~ManagedBuffer() {
    cleanup();
}

ManagedBuffer::ManagedBuffer(ManagedBuffer&& other) noexcept
    : ManagedResource(std::move(other))
    , buffer_(other.buffer_)
    , memory_(other.memory_)
    , size_(other.size_)
    , mappedData_(other.mappedData_) {
    
    // Clear the moved-from object
    other.buffer_ = VK_NULL_HANDLE;
    other.memory_ = VK_NULL_HANDLE;
    other.size_ = 0;
    other.mappedData_ = nullptr;
}

ManagedBuffer& ManagedBuffer::operator=(ManagedBuffer&& other) noexcept {
    if (this != &other) {
        cleanup();
        
        ManagedResource::operator=(std::move(other));
        buffer_ = other.buffer_;
        memory_ = other.memory_;
        size_ = other.size_;
        mappedData_ = other.mappedData_;
        
        // Clear the moved-from object
        other.buffer_ = VK_NULL_HANDLE;
        other.memory_ = VK_NULL_HANDLE;
        other.size_ = 0;
        other.mappedData_ = nullptr;
    }
    return *this;
}

void* ManagedBuffer::map() {
    if (mappedData_ == nullptr) {
        if (vkMapMemory(device_, memory_, 0, size_, 0, &mappedData_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map buffer memory: " + debugName_);
        }
    }
    return mappedData_;
}

void ManagedBuffer::unmap() {
    if (mappedData_ != nullptr) {
        vkUnmapMemory(device_, memory_);
        mappedData_ = nullptr;
    }
}

void ManagedBuffer::updateData(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    void* mappedMem = map();
    memcpy(static_cast<char*>(mappedMem) + offset, data, size);
    unmap();
}

void ManagedBuffer::cleanup() {
    unmap();
    if (buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buffer_, nullptr);
        buffer_ = VK_NULL_HANDLE;
    }
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }
}

// ============================================================================
// ManagedImage Implementation  
// ============================================================================

ManagedImage::ManagedImage(VkDevice device, uint32_t width, uint32_t height, VkFormat format,
                           VkImageTiling tiling, VkImageUsageFlags usage, uint32_t memoryTypeIndex,
                           VkImageAspectFlags aspectFlags, const std::string& debugName)
    : ManagedResource(device, debugName), format_(format), width_(width), height_(height) {
    
    // Create the image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateImage(device_, &imageInfo, nullptr, &image_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image: " + debugName);
    }
    
    // Get memory requirements and allocate
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_, image_, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory_) != VK_SUCCESS) {
        vkDestroyImage(device_, image_, nullptr);
        throw std::runtime_error("Failed to allocate image memory: " + debugName);
    }
    
    vkBindImageMemory(device_, image_, memory_, 0);
    
    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(device_, &viewInfo, nullptr, &imageView_) != VK_SUCCESS) {
        vkFreeMemory(device_, memory_, nullptr);
        vkDestroyImage(device_, image_, nullptr);
        throw std::runtime_error("Failed to create image view: " + debugName);
    }
}

ManagedImage::~ManagedImage() {
    cleanup();
}

ManagedImage::ManagedImage(ManagedImage&& other) noexcept
    : ManagedResource(std::move(other))
    , image_(other.image_)
    , memory_(other.memory_)
    , imageView_(other.imageView_)
    , format_(other.format_)
    , width_(other.width_)
    , height_(other.height_) {
    
    // Clear the moved-from object
    other.image_ = VK_NULL_HANDLE;
    other.memory_ = VK_NULL_HANDLE;
    other.imageView_ = VK_NULL_HANDLE;
}

ManagedImage& ManagedImage::operator=(ManagedImage&& other) noexcept {
    if (this != &other) {
        cleanup();
        
        ManagedResource::operator=(std::move(other));
        image_ = other.image_;
        memory_ = other.memory_;
        imageView_ = other.imageView_;
        format_ = other.format_;
        width_ = other.width_;
        height_ = other.height_;
        
        // Clear the moved-from object
        other.image_ = VK_NULL_HANDLE;
        other.memory_ = VK_NULL_HANDLE;
        other.imageView_ = VK_NULL_HANDLE;
    }
    return *this;
}

void ManagedImage::cleanup() {
    if (imageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, imageView_, nullptr);
        imageView_ = VK_NULL_HANDLE;
    }
    if (image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, image_, nullptr);
        image_ = VK_NULL_HANDLE;
    }
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }
}

// ============================================================================
// ResourceManager Implementation
// ============================================================================

ResourceManager::ResourceManager(VkDevice device, VkPhysicalDevice physicalDevice)
    : device_(device), physicalDevice_(physicalDevice) {
    std::cout << "[RESOURCE MANAGER] Initialized for device" << std::endl;
}

ResourceManager::~ResourceManager() {
    std::cout << "[RESOURCE MANAGER] Cleanup - " << bufferCount_ << " buffers, " 
              << imageCount_ << " images, " << totalAllocatedMemory_ << " bytes allocated" << std::endl;
}

std::unique_ptr<ManagedBuffer> ResourceManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                                             VkMemoryPropertyFlags properties, 
                                                             const std::string& debugName) {
    try {
        // Create a temporary buffer to get memory requirements
        VkBuffer tempBuffer;
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateBuffer(device_, &bufferInfo, nullptr, &tempBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create temporary buffer for memory type query");
        }
        
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device_, tempBuffer, &memRequirements);
        vkDestroyBuffer(device_, tempBuffer, nullptr);
        
        // Find appropriate memory type
        uint32_t memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
        
        auto buffer = std::make_unique<ManagedBuffer>(device_, size, usage, memoryTypeIndex, debugName);
        bufferCount_++;
        totalAllocatedMemory_ += size;
        return buffer;
    } catch (const std::exception& e) {
        std::cerr << "[RESOURCE MANAGER] Buffer creation failed: " << e.what() << std::endl;
        throw;
    }
}

std::unique_ptr<ManagedImage> ResourceManager::createImage(uint32_t width, uint32_t height, VkFormat format,
                                                           VkImageTiling tiling, VkImageUsageFlags usage,
                                                           VkMemoryPropertyFlags properties,
                                                           VkImageAspectFlags aspectFlags, 
                                                           const std::string& debugName) {
    try {
        // Create a temporary image to get memory requirements
        VkImage tempImage;
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateImage(device_, &imageInfo, nullptr, &tempImage) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create temporary image for memory type query");
        }
        
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device_, tempImage, &memRequirements);
        vkDestroyImage(device_, tempImage, nullptr);
        
        // Find appropriate memory type
        uint32_t memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
        
        auto image = std::make_unique<ManagedImage>(device_, width, height, format, tiling, usage, 
                                                    memoryTypeIndex, aspectFlags, debugName);
        imageCount_++;
        totalAllocatedMemory_ += width * height * 4; // Rough estimate
        return image;
    } catch (const std::exception& e) {
        std::cerr << "[RESOURCE MANAGER] Image creation failed: " << e.what() << std::endl;
        throw;
    }
}

uint32_t ResourceManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type!");
}

void ResourceManager::printResourceSummary() const {
    std::cout << "\n=== RESOURCE MANAGER SUMMARY ===" << std::endl;
    std::cout << "Buffers: " << bufferCount_ << std::endl;
    std::cout << "Images: " << imageCount_ << std::endl;
    std::cout << "Total Allocated Memory: " << totalAllocatedMemory_ << " bytes" << std::endl;
    std::cout << "==============================\n" << std::endl;
}

} // namespace VulkanMon