#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "../core/Window.h"
#include "../core/Camera.h"
#include "ResourceManager.h"
#include "../io/AssetManager.h"
#include "../io/ModelLoader.h"
#include "../systems/LightingSystem.h"
#include "../systems/MaterialSystem.h"
#include "../utils/Logger.h"

#include <memory>
#include <vector>
#include <chrono>
#include <glm/glm.hpp>

// Forward declarations and structures
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 cameraPos;
    float _padding;
};

/**
 * VulkanMon Vulkan Rendering System
 * 
 * Extracted from HelloTriangleApp to create focused, single-responsibility rendering system.
 * Handles all Vulkan operations including initialization, resource management, and frame rendering.
 * 
 * Responsibilities:
 * - Vulkan instance and device management
 * - Swapchain creation and management
 * - Graphics pipeline setup and configuration
 * - Command buffer recording and submission
 * - Frame synchronization and presentation
 * - Resource lifetime management (buffers, images, descriptors)
 * 
 * Design Philosophy:
 * - Single responsibility: Vulkan rendering only
 * - Clean interfaces: Modern std::function callbacks for external systems
 * - RAII compliance: Automatic resource cleanup
 * - Testable: Can be mocked and unit tested
 * - Performance focused: Optimized for real-time rendering
 */

namespace VulkanMon {

// Forward declarations
class Model;

class VulkanRenderer {
public:
    // Callback types for operations VulkanRenderer cannot perform directly
    using ShaderReloadCallback = std::function<void()>;
    using FrameUpdateCallback = std::function<void(float deltaTime)>;
    
    /**
     * Create VulkanRenderer with required dependencies
     * 
     * @param window Window system for surface creation
     * @param camera Camera system for view matrices
     * @param resourceManager Resource management system
     * @param assetManager Asset loading system
     * @param modelLoader 3D model loading system
     * @param lightingSystem Lighting calculation system
     * @param materialSystem Material property system
     */
    VulkanRenderer(
        std::shared_ptr<Window> window,
        std::shared_ptr<Camera> camera,
        std::shared_ptr<ResourceManager> resourceManager,
        std::shared_ptr<AssetManager> assetManager,
        std::shared_ptr<ModelLoader> modelLoader,
        std::shared_ptr<LightingSystem> lightingSystem,
        std::shared_ptr<MaterialSystem> materialSystem
    );
    
    /**
     * Destructor - automatic Vulkan resource cleanup
     */
    ~VulkanRenderer();
    
    // Move-only semantics (RAII compliance)
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    VulkanRenderer(VulkanRenderer&&) = default;
    VulkanRenderer& operator=(VulkanRenderer&&) = default;
    
    /**
     * Initialize Vulkan rendering system
     * Must be called before any rendering operations
     * 
     * @throws std::runtime_error if Vulkan initialization fails
     */
    void initialize();
    
    /**
     * Render a single frame
     * Call once per frame in main loop
     * 
     * @param deltaTime Time since last frame in seconds
     */
    void renderFrame(float deltaTime);
    
    /**
     * Handle window resize events
     * Recreates swapchain and dependent resources
     */
    void handleWindowResize();
    
    /**
     * Reload shaders from disk
     * Useful for development and hot-reloading
     * 
     * @throws std::runtime_error if shader compilation fails
     */
    void reloadShaders();
    
    /**
     * Set current 3D model to render
     * 
     * @param model Model to render (can be nullptr for no model)
     */
    void setCurrentModel(std::shared_ptr<Model> model);
    
    /**
     * Cycle through material presets (temporary - will move to game objects in Phase 6)
     */
    void cycleMaterialPreset();
    
    /**
     * Register callback for frame updates
     * Called before each frame render for external system updates
     * 
     * @param callback Function to call for frame updates
     */
    void setFrameUpdateCallback(FrameUpdateCallback callback);
    
    /**
     * Check if renderer is ready for rendering
     * 
     * @return true if initialization completed successfully
     */
    bool isInitialized() const { return initialized_; }
    
    /**
     * Get Vulkan device handle
     * Used by other systems that need direct Vulkan access
     * 
     * @return Vulkan device handle
     */
    VkDevice getDevice() const { return device_; }
    
    /**
     * Get Vulkan instance handle
     * Used by other systems that need direct Vulkan access
     * 
     * @return Vulkan instance handle
     */
    VkInstance getInstance() const { return instance_; }
    
    /**
     * Get current frame statistics
     * 
     * @return Frame time in milliseconds
     */
    float getFrameTime() const { return lastFrameTime_; }

private:
    // System references (not owned)
    std::shared_ptr<Window> window_;
    std::shared_ptr<Camera> camera_;
    std::shared_ptr<ResourceManager> resourceManager_;
    std::shared_ptr<AssetManager> assetManager_;
    std::shared_ptr<ModelLoader> modelLoader_;
    std::shared_ptr<LightingSystem> lightingSystem_;
    std::shared_ptr<MaterialSystem> materialSystem_;
    
    // Current model being rendered
    std::shared_ptr<Model> currentModel_;
    
    // Frame update callback
    FrameUpdateCallback frameUpdateCallback_;
    
    // Material state
    MaterialData currentMaterialData_;
    
    // Initialization state
    bool initialized_ = false;
    
    // Core Vulkan context
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    
    // Swapchain and presentation
    VkSwapchainKHR swapChain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages_;
    VkFormat swapChainImageFormat_;
    VkExtent2D swapChainExtent_;
    std::vector<VkImageView> swapChainImageViews_;
    std::vector<VkFramebuffer> swapChainFramebuffers_;
    
    // Graphics pipeline
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkShaderModule vertShaderModule_ = VK_NULL_HANDLE;
    VkShaderModule fragShaderModule_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    
    // Command processing
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
    
    // Synchronization
    VkSemaphore imageAvailableSemaphore_ = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore_ = VK_NULL_HANDLE;
    VkFence inFlightFence_ = VK_NULL_HANDLE;
    
    // Texture resources
    VkImage textureImage_ = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory_ = VK_NULL_HANDLE;
    VkImageView textureImageView_ = VK_NULL_HANDLE;
    VkSampler textureSampler_ = VK_NULL_HANDLE;
    
    // Descriptor resources
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
    
    // Uniform Buffer Object resources
    VkBuffer uniformBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory uniformBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer materialBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory materialBufferMemory_ = VK_NULL_HANDLE;
    
    // Depth buffer resources
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    
    // Frame timing
    std::chrono::high_resolution_clock::time_point lastFrameTimePoint_;
    float lastFrameTime_ = 0.0f;
    
    // Vulkan initialization methods
    void initVulkan();
    void createInstance();
    void createSurface();
    void createLogicalDevice();
    void createSwapChain();
    void createRenderPass();
    void createShaderModules();
    void createGraphicsPipeline();
    
    // Shader helper methods
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    
    // Resource creation methods
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                     VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, 
                    VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    void createDescriptorSetLayout();
    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();
    void createDescriptorPool();
    void createDescriptorSet();
    void createUniformBuffer();
    void createMaterialBuffer();
    void createDepthResources();
    
    // Rendering methods
    void drawFrame();
    void recordCommandBuffers();
    void updateUniformBuffer();
    void updateMaterialBuffer();
    
    // Helper methods
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    int findGraphicsQueueFamily();
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    bool hasStencilComponent(VkFormat format);
    
    // Cleanup methods
    void cleanup();
    void cleanupSwapChain();
    
    // Debug and logging helpers
    void logRenderingState(const std::string& operation) const;
    
    // Temporary system initialization (will be moved to VulkanContext)
    void initializeCoreSystemsTemporary();
    
    // Test model loading
    void loadTestModel();
};

} // namespace VulkanMon