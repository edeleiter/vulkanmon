#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>

/**
 * VulkanTestFixture - Mock Vulkan Context for Unit Testing
 * 
 * Provides minimal Vulkan setup for testing VulkanMon classes that require
 * Vulkan context (ResourceManager, AssetManager, ModelLoader).
 * 
 * Following our philosophy:
 * - "Simple is Powerful" - Minimal setup, just what's needed for tests
 * - "Test, Test, Test" - Reliable test environment setup/teardown
 * - "Document Often" - Clear usage patterns and limitations
 */

namespace VulkanMon {
namespace Testing {

/**
 * Simple RAII wrapper for basic Vulkan instance needed for testing
 * This is a minimal implementation - just enough to create devices and test our classes
 */
class VulkanTestContext {
public:
    VulkanTestContext();
    ~VulkanTestContext();
    
    // No copy/move - testing fixture should be created per test
    VulkanTestContext(const VulkanTestContext&) = delete;
    VulkanTestContext& operator=(const VulkanTestContext&) = delete;
    
    // Accessors for our VulkanMon classes
    VkInstance getInstance() const { return instance_; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice_; }
    VkDevice getDevice() const { return device_; }
    VkQueue getGraphicsQueue() const { return graphicsQueue_; }
    VkCommandPool getCommandPool() const { return commandPool_; }
    
    // Check if context is valid (for tests that need real Vulkan)
    bool isValid() const { return instance_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE; }
    
    // Get an error message if context creation failed
    const std::string& getErrorMessage() const { return errorMessage_; }

private:
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    
    std::string errorMessage_;
    
    // Helper methods
    bool createInstance();
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool createCommandPool();
    
    void cleanup();
    
    // Find graphics queue family
    int findGraphicsQueueFamily();
    int findGraphicsQueueFamily(VkPhysicalDevice device);
};

/**
 * Test fixture base class for tests that need Vulkan context
 * Use this as a base class for test cases that need real Vulkan resources
 */
class VulkanTestFixture {
public:
    VulkanTestFixture();
    virtual ~VulkanTestFixture() = default;
    
protected:
    std::unique_ptr<VulkanTestContext> vulkanContext_;
    
    // Helper method to skip tests if Vulkan is not available
    bool skipIfNoVulkan() const {
        return !vulkanContext_->isValid();
    }
    
    // Convenience accessors
    VkDevice getDevice() const { return vulkanContext_->getDevice(); }
    VkPhysicalDevice getPhysicalDevice() const { return vulkanContext_->getPhysicalDevice(); }
    VkCommandPool getCommandPool() const { return vulkanContext_->getCommandPool(); }
    VkQueue getGraphicsQueue() const { return vulkanContext_->getGraphicsQueue(); }
};

/**
 * Lightweight fixture for tests that don't need real Vulkan context
 * Use this for testing Logger, basic utility functions, etc.
 */
class LightweightTestFixture {
public:
    LightweightTestFixture() = default;
    virtual ~LightweightTestFixture() = default;
    
protected:
    // Helper method to create temporary test files
    std::string createTempFile(const std::string& content = "");
    
    // Helper method to cleanup test files
    void cleanupTempFiles();
    
private:
    std::vector<std::string> tempFiles_;
};

} // namespace Testing
} // namespace VulkanMon