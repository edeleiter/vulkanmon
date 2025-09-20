#pragma once
#include <vulkan/vulkan.h>

namespace VulkanMon {

/**
 * RAII wrapper for Vulkan mapped GPU memory
 *
 * Ensures automatic unmapping when the object goes out of scope,
 * preventing memory leaks and dangling pointers.
 *
 * Usage:
 *   auto mapped = MappedBuffer(device, memory, mappedPtr);
 *   memcpy(mapped.get(), data, size);
 *   // Automatic unmap on destruction
 */
class MappedBuffer {
public:
    MappedBuffer() = default;

    MappedBuffer(VkDevice device, VkDeviceMemory memory, void* mapped)
        : device_(device), memory_(memory), mapped_(mapped) {}

    ~MappedBuffer() {
        unmap();
    }

    // Move-only semantics (prevent accidental copying)
    MappedBuffer(const MappedBuffer&) = delete;
    MappedBuffer& operator=(const MappedBuffer&) = delete;

    MappedBuffer(MappedBuffer&& other) noexcept
        : device_(other.device_), memory_(other.memory_), mapped_(other.mapped_) {
        other.device_ = VK_NULL_HANDLE;
        other.memory_ = VK_NULL_HANDLE;
        other.mapped_ = nullptr;
    }

    MappedBuffer& operator=(MappedBuffer&& other) noexcept {
        if (this != &other) {
            unmap();
            device_ = other.device_;
            memory_ = other.memory_;
            mapped_ = other.mapped_;
            other.device_ = VK_NULL_HANDLE;
            other.memory_ = VK_NULL_HANDLE;
            other.mapped_ = nullptr;
        }
        return *this;
    }

    // Access mapped memory pointer
    void* get() const { return mapped_; }

    // Check if mapping is valid
    bool isValid() const { return mapped_ != nullptr; }

    // Explicit unmap (automatic on destruction)
    void unmap() {
        if (mapped_ && device_ != VK_NULL_HANDLE && memory_ != VK_NULL_HANDLE) {
            vkUnmapMemory(device_, memory_);
            mapped_ = nullptr;
        }
    }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    void* mapped_ = nullptr;
};

} // namespace VulkanMon