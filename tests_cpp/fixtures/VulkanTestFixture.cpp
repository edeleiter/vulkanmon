#include "VulkanTestFixture.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <set>

namespace VulkanMon {
namespace Testing {

// ============================================================================
// VulkanTestContext Implementation (Full Vulkan context for testing)
// ============================================================================

VulkanTestContext::VulkanTestContext() {
    // Try to create a real Vulkan context for testing
    if (!createInstance()) {
        errorMessage_ = "Failed to create Vulkan instance";
        return;
    }
    
    if (!selectPhysicalDevice()) {
        errorMessage_ = "Failed to find suitable physical device";
        return;
    }
    
    if (!createLogicalDevice()) {
        errorMessage_ = "Failed to create logical device";
        return;
    }
    
    if (!createCommandPool()) {
        errorMessage_ = "Failed to create command pool";
        return;
    }
    
    // If we get here, context is valid
    errorMessage_ = "";
}

VulkanTestContext::~VulkanTestContext() {
    cleanup();
}

bool VulkanTestContext::createInstance() {
    // Application info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanMon Unit Tests";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VulkanMon Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // Create info for instance
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // No extensions needed for testing (headless)
    createInfo.enabledExtensionCount = 0;
    createInfo.ppEnabledExtensionNames = nullptr;

    // No validation layers for testing (keep it simple)
    createInfo.enabledLayerCount = 0;

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
    return result == VK_SUCCESS;
}

bool VulkanTestContext::selectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

    if (deviceCount == 0) {
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    // For testing, just pick the first available device
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        // Any device that has a graphics queue will work for testing
        int graphicsFamily = findGraphicsQueueFamily(device);
        if (graphicsFamily >= 0) {
            physicalDevice_ = device;
            return true;
        }
    }

    return false;
}

bool VulkanTestContext::createLogicalDevice() {
    int graphicsFamily = findGraphicsQueueFamily(physicalDevice_);
    if (graphicsFamily < 0) {
        return false;
    }

    // Queue create info
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsFamily;
    queueCreateInfo.queueCount = 1;

    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    // Device features (none needed for basic testing)
    VkPhysicalDeviceFeatures deviceFeatures{};

    // Device create info
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;

    // No extensions needed for testing
    createInfo.enabledExtensionCount = 0;
    createInfo.enabledLayerCount = 0;

    VkResult result = vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_);
    if (result != VK_SUCCESS) {
        return false;
    }

    // Get graphics queue
    vkGetDeviceQueue(device_, graphicsFamily, 0, &graphicsQueue_);
    return true;
}

bool VulkanTestContext::createCommandPool() {
    int graphicsFamily = findGraphicsQueueFamily(physicalDevice_);
    if (graphicsFamily < 0) {
        return false;
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily;

    VkResult result = vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_);
    return result == VK_SUCCESS;
}

void VulkanTestContext::cleanup() {
    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }

    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

int VulkanTestContext::findGraphicsQueueFamily() {
    return findGraphicsQueueFamily(physicalDevice_);
}

int VulkanTestContext::findGraphicsQueueFamily(VkPhysicalDevice device) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            return i;
        }
        i++;
    }

    return -1;
}

// ============================================================================
// VulkanTestFixture Implementation
// ============================================================================

VulkanTestFixture::VulkanTestFixture() 
    : vulkanContext_(std::make_unique<VulkanTestContext>()) {
    // Base constructor - context created above
}

} // namespace Testing
} // namespace VulkanMon