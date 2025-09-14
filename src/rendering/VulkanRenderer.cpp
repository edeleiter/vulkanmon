#include "VulkanRenderer.h"
#include "../utils/Utils.h"
#include "../io/ModelLoader.h"
#include "../systems/MaterialSystem.h"
#include "../utils/Logger.h"
#include <iostream>
#include <stdexcept>
#include <array>
#include <filesystem>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

using namespace VulkanMon;

// Shader file constants
constexpr const char* VERTEX_SHADER_COMPILED = "shaders/vert.spv";
constexpr const char* FRAGMENT_SHADER_COMPILED = "shaders/frag.spv";

VulkanRenderer::VulkanRenderer(
    std::shared_ptr<Window> window,
    std::shared_ptr<Camera> camera,
    std::shared_ptr<ResourceManager> resourceManager,
    std::shared_ptr<AssetManager> assetManager,
    std::shared_ptr<ModelLoader> modelLoader,
    std::shared_ptr<LightingSystem> lightingSystem,
    std::shared_ptr<MaterialSystem> materialSystem
) : window_(window),
    camera_(camera),
    resourceManager_(resourceManager),
    assetManager_(assetManager),
    modelLoader_(modelLoader),
    lightingSystem_(lightingSystem),
    materialSystem_(materialSystem) {
    
    VKMON_INFO("VulkanRenderer created");
}

VulkanRenderer::~VulkanRenderer() {
    if (initialized_) {
        cleanupImGui();
        cleanup();
    }
    VKMON_INFO("VulkanRenderer destroyed");
}

void VulkanRenderer::initialize() {
    if (initialized_) {
        VKMON_WARNING("VulkanRenderer already initialized");
        return;
    }
    
    try {
        VKMON_INFO("Initializing Vulkan renderer...");
        
        // Initialize Vulkan subsystems
        initVulkan();
        
        initialized_ = true;
        VKMON_INFO("VulkanRenderer initialization completed successfully");

        // Initialize ImGui after Vulkan initialization
        if (imguiEnabled_) {
            initializeImGui();
            VKMON_INFO("ImGui initialization completed successfully");
        }
        
    } catch (const std::exception& e) {
        VKMON_ERROR("VulkanRenderer initialization failed: " + std::string(e.what()));
        cleanup();
        throw;
    }
}

void VulkanRenderer::renderFrame(float deltaTime) {
    if (!initialized_) {
        VKMON_WARNING("Cannot render frame: VulkanRenderer not initialized");
        return;
    }
    
    // Update frame timing
    auto currentTime = std::chrono::high_resolution_clock::now();
    if (lastFrameTimePoint_.time_since_epoch().count() != 0) {
        lastFrameTime_ = std::chrono::duration<float, std::milli>(currentTime - lastFrameTimePoint_).count();
    }
    lastFrameTimePoint_ = currentTime;
    
    // Call external frame update callback
    if (frameUpdateCallback_) {
        frameUpdateCallback_(deltaTime);
    }
    
    // Render the frame
    drawFrame();
}

void VulkanRenderer::handleWindowResize(int width, int height) {
    VKMON_INFO("Handling window resize to " + std::to_string(width) + "x" + std::to_string(height));

    // Don't resize if minimized (width/height = 0)
    if (width == 0 || height == 0) {
        VKMON_INFO("Window minimized, skipping swapchain recreation");
        return;
    }

    // Wait for device to be idle before recreating swapchain
    vkDeviceWaitIdle(device_);

    // Recreate swapchain with new dimensions
    recreateSwapChain();

    // Update ImGui display size
    if (imguiInitialized_) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
        VKMON_INFO("Updated ImGui display size");
    }

    VKMON_INFO("Window resize handling completed successfully");
}

void VulkanRenderer::reloadShaders() {
    if (!initialized_) {
        throw std::runtime_error("Cannot reload shaders: VulkanRenderer not initialized");
    }
    
    try {
        // 1. Recompile shader files
        if (!Utils::recompileShaders()) {
            throw std::runtime_error("Shader recompilation failed");
        }
        
        // 2. Wait for device to be idle
        vkDeviceWaitIdle(device_);
        
        // 3. Destroy old pipeline and shader modules
        if (graphicsPipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, graphicsPipeline_, nullptr);
            graphicsPipeline_ = VK_NULL_HANDLE;
        }
        if (vertShaderModule_ != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, vertShaderModule_, nullptr);
            vertShaderModule_ = VK_NULL_HANDLE;
        }
        if (fragShaderModule_ != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, fragShaderModule_, nullptr);
            fragShaderModule_ = VK_NULL_HANDLE;
        }
        
        // 4. Recreate shader modules and pipeline
        createShaderModules();
        createGraphicsPipeline();
        
        VKMON_INFO("Shader reload completed successfully");
        
    } catch (const std::exception& e) {
        VKMON_ERROR("Shader reload failed: " + std::string(e.what()));
        throw;
    }
}

void VulkanRenderer::setCurrentModel(std::shared_ptr<Model> model) {
    currentModel_ = model;
    
    if (model) {
        VKMON_INFO("Current model set: " + std::string(model ? "valid model" : "nullptr"));
    } else {
        VKMON_INFO("Current model cleared");
    }
}

void VulkanRenderer::setFrameUpdateCallback(FrameUpdateCallback callback) {
    frameUpdateCallback_ = callback;
}

void VulkanRenderer::setECSRenderCallback(ECSRenderCallback callback) {
    ecsRenderCallback_ = callback;
}

// =============================================================================
// ECS Integration Interface (Phase 6.2)
// =============================================================================

void VulkanRenderer::beginECSFrame() {
    if (ecsFrameActive_) {
        VKMON_ERROR("ECS frame already active! Call endECSFrame() before beginning a new frame.");
        return;
    }

    // Wait for previous frame
    vkWaitForFences(device_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &inFlightFence_);

    // Acquire next swapchain image
    VkResult result = vkAcquireNextImageKHR(device_, swapChain_, UINT64_MAX,
                                           imageAvailableSemaphore_, VK_NULL_HANDLE,
                                           &currentImageIndex_);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        handleWindowResize(window_->getWidth(), window_->getHeight());
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image!");
    }

    // Update frame-level uniforms (view, projection, lighting)
    updateUniformBuffer();
    updateMaterialBuffer();

    // Reset command buffer and begin recording
    vkResetCommandBuffer(commandBuffers_[currentImageIndex_], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(commandBuffers_[currentImageIndex_], &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = swapChainFramebuffers_[currentImageIndex_];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent_;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffers_[currentImageIndex_], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind graphics pipeline
    vkCmdBindPipeline(commandBuffers_[currentImageIndex_], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

    // Set viewport dynamically (essential for proper resize handling)
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent_.width);
    viewport.height = static_cast<float>(swapChainExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffers_[currentImageIndex_], 0, 1, &viewport);

    // Set scissor rectangle
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent_;
    vkCmdSetScissor(commandBuffers_[currentImageIndex_], 0, 1, &scissor);

    // Bind global descriptor set (set 0: UBO, texture, lighting)
    vkCmdBindDescriptorSets(commandBuffers_[currentImageIndex_], VK_PIPELINE_BIND_POINT_GRAPHICS,
                           pipelineLayout_, 0, 1, &globalDescriptorSet_, 0, nullptr);

    ecsFrameActive_ = true;
    frameLoadedMeshes_.clear();

    VKMON_DEBUG("ECS frame begun, ready for object rendering");
}

void VulkanRenderer::renderECSObject(const glm::mat4& modelMatrix,
                                    const std::string& meshPath,
                                    uint32_t materialId) {
    if (!ecsFrameActive_) {
        VKMON_ERROR("ECS frame not active! Call beginECSFrame() first.");
        return;
    }

    // Ensure mesh is loaded
    ensureMeshLoaded(meshPath);

    // Bind material-specific descriptor set (set 1)
    if (materialSystem_ && materialId < materialSystem_->getMaterialCount()) {
        VkDescriptorSet materialDescriptorSet = materialSystem_->getDescriptorSet(materialId);
        if (materialDescriptorSet != VK_NULL_HANDLE) {
            // Bind the material descriptor set to set 1
            vkCmdBindDescriptorSets(commandBuffers_[currentImageIndex_], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   pipelineLayout_, 1, 1, &materialDescriptorSet, 0, nullptr);
        }
    }

    // Update push constants with model matrix
    PushConstants pushConstants{};
    pushConstants.model = modelMatrix;

    vkCmdPushConstants(commandBuffers_[currentImageIndex_], pipelineLayout_,
                      VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);

    // Render the specific model for this meshPath
    auto modelIt = modelCache_.find(meshPath);
    if (modelIt != modelCache_.end() && modelIt->second && !modelIt->second->meshes.empty()) {
        auto model = modelIt->second;
        for (const auto& mesh : model->meshes) {
            if (mesh->vertexBuffer && mesh->indexBuffer) {
                VkBuffer vertexBuffers[] = {mesh->vertexBuffer->getBuffer()};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffers_[currentImageIndex_], 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffers_[currentImageIndex_], mesh->indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexed(commandBuffers_[currentImageIndex_], static_cast<uint32_t>(mesh->indices.size()), 1, 0, 0, 0);
            }
        }
        VKMON_DEBUG("Rendered ECS object with mesh: " + meshPath);
    } else {
        VKMON_WARNING("No model cached for meshPath: " + meshPath);
    }
}

void VulkanRenderer::endECSFrame() {
    if (!ecsFrameActive_) {
        VKMON_ERROR("ECS frame not active! Call beginECSFrame() first.");
        return;
    }

    // Render ImGui if enabled
    if (imguiEnabled_ && imguiInitialized_) {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffers_[currentImageIndex_]);
    }

    // End render pass
    vkCmdEndRenderPass(commandBuffers_[currentImageIndex_]);

    // End command buffer recording
    if (vkEndCommandBuffer(commandBuffers_[currentImageIndex_]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphore_};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentImageIndex_];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphore_};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFence_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    // Present the frame
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain_};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &currentImageIndex_;

    VkResult result = vkQueuePresentKHR(graphicsQueue_, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        handleWindowResize(window_->getWidth(), window_->getHeight());
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image!");
    }

    ecsFrameActive_ = false;

    VKMON_DEBUG("ECS frame completed and presented");
}

void VulkanRenderer::ensureMeshLoaded(const std::string& meshPath) {
    // Check if model is already cached
    if (modelCache_.find(meshPath) != modelCache_.end()) {
        return; // Already loaded
    }

    // Load the model using ModelLoader
    if (modelLoader_) {
        try {
            VKMON_DEBUG("Loading model: " + meshPath);
            auto model = modelLoader_->loadModel(meshPath);

            if (model && !model->meshes.empty()) {
                modelCache_[meshPath] = std::move(model);
                VKMON_INFO("Model loaded and cached: " + meshPath);
            } else {
                VKMON_WARNING("Failed to load model or model is empty: " + meshPath);
                // Use fallback - add current model as cache for this path
                if (currentModel_) {
                    modelCache_[meshPath] = currentModel_;
                    VKMON_WARNING("Using fallback model for: " + meshPath);
                }
            }
        } catch (const std::exception& e) {
            VKMON_ERROR("Exception loading model " + meshPath + ": " + std::string(e.what()));
            // Use fallback
            if (currentModel_) {
                modelCache_[meshPath] = currentModel_;
                VKMON_WARNING("Using fallback model due to exception: " + meshPath);
            }
        }
    } else {
        VKMON_ERROR("ModelLoader not available for loading: " + meshPath);
    }
}

// =============================================================================
// Vulkan Initialization Methods (extracted from main.cpp)
// =============================================================================

void VulkanRenderer::initVulkan() {
    VKMON_DEBUG("Starting Vulkan initialization sequence...");
    
    createInstance();
    createSurface();
    createLogicalDevice();  // This will include physical device selection
    createSwapChain();
    createRenderPass();
    createDescriptorSetLayout();
    createShaderModules();
    createGraphicsPipeline();
    createDepthResources();
    createFramebuffers();
    createCommandPool();
    
    // Initialize core systems (after command pool creation)
    // TODO: This is temporary - will be moved to VulkanContext
    initializeCoreSystemsTemporary();
    
    // Load test model
    loadTestModel();
    
    createUniformBuffer();
    createMaterialBuffer();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    createDescriptorPool();
    createDescriptorSet();
    createGlobalDescriptorPool();
    createGlobalDescriptorSet();
    createCommandBuffers();
    recordCommandBuffers();
    createSyncObjects();
    
    VKMON_INFO("Vulkan initialization completed");
}

void VulkanRenderer::createInstance() {
    VKMON_DEBUG("Creating Vulkan instance...");
    
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanMon";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VulkanMon Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Get required extensions for GLFW
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledLayerCount = 0;

    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }

    VKMON_DEBUG("Vulkan instance created successfully");
}

void VulkanRenderer::createSurface() {
    VKMON_DEBUG("Creating Vulkan surface...");
    
    window_->createSurface(instance_);
    surface_ = window_->getSurface();
    
    VKMON_DEBUG("Window surface created successfully");
}

void VulkanRenderer::createLogicalDevice() {
    VKMON_INFO("Selecting physical device and creating logical device...");
    
    // Pick physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    // Simple selection: just pick the first device
    physicalDevice_ = devices[0];

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice_, &deviceProperties);
    VKMON_INFO("Selected GPU: " + std::string(deviceProperties.deviceName));
    
    // Find graphics queue family
    int graphicsQueueFamily = findGraphicsQueueFamily();
    if (graphicsQueueFamily == -1) {
        throw std::runtime_error("Failed to find graphics queue family!");
    }

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;

    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

    vkGetDeviceQueue(device_, graphicsQueueFamily, 0, &graphicsQueue_);
    VKMON_DEBUG("Logical device created successfully");
}

int VulkanRenderer::findGraphicsQueueFamily() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, queueFamilies.data());

    // Find a queue family that supports graphics
    for (int i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            return i;
        }
    }
    return -1;
}

void VulkanRenderer::createSwapChain() {
    VKMON_DEBUG("Creating swapchain...");
    
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &capabilities);

    // Choose swap surface format (prefer SRGB if available)
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }

    // Choose extent based on window dimensions
    if (capabilities.currentExtent.width != UINT32_MAX) {
        swapChainExtent_ = capabilities.currentExtent;
    } else {
        // Get window extent from Window system
        swapChainExtent_ = window_->getExtent();
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapChainExtent_;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // V-Sync
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapChain_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }

    // Get swap chain images
    vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, nullptr);
    swapChainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, swapChainImages_.data());

    swapChainImageFormat_ = surfaceFormat.format;
    VKMON_INFO("Swap chain created successfully with " + std::to_string(imageCount) + " images");
}

void VulkanRenderer::recreateSwapChain() {
    VKMON_INFO("Recreating swap chain for resize...");

    // Wait for any in-flight operations to complete
    vkDeviceWaitIdle(device_);

    // Clean up old swapchain-dependent resources
    // Cleanup framebuffers
    for (size_t i = 0; i < swapChainFramebuffers_.size(); i++) {
        vkDestroyFramebuffer(device_, swapChainFramebuffers_[i], nullptr);
    }

    // Cleanup image views
    for (size_t i = 0; i < swapChainImageViews_.size(); i++) {
        vkDestroyImageView(device_, swapChainImageViews_[i], nullptr);
    }

    // Cleanup depth resources
    vkDestroyImageView(device_, depthImageView_, nullptr);
    vkDestroyImage(device_, depthImage_, nullptr);
    vkFreeMemory(device_, depthImageMemory_, nullptr);

    // Cleanup old swapchain
    vkDestroySwapchainKHR(device_, swapChain_, nullptr);

    // Recreate swapchain with new window size
    createSwapChain();

    // Recreate swapchain image views
    swapChainImageViews_.resize(swapChainImages_.size());
    for (size_t i = 0; i < swapChainImages_.size(); i++) {
        swapChainImageViews_[i] = createImageView(swapChainImages_[i], swapChainImageFormat_, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    // Recreate depth buffer
    VkFormat depthFormat = findDepthFormat();
    createImage(swapChainExtent_.width, swapChainExtent_.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               depthImage_, depthImageMemory_);
    depthImageView_ = createImageView(depthImage_, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    // Recreate framebuffers
    createFramebuffers();

    VKMON_INFO("Swap chain recreation completed successfully");
}

void VulkanRenderer::createRenderPass() {
    VKMON_INFO("Creating render pass...");
    
    // Color attachment (what we render to)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment (for 3D depth testing)
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }

    VKMON_DEBUG("Render pass created successfully");
}

void VulkanRenderer::createShaderModules() {
    VKMON_DEBUG("Creating shader modules...");
    
    auto vertShaderCode = Utils::readFile(VERTEX_SHADER_COMPILED);
    auto fragShaderCode = Utils::readFile(FRAGMENT_SHADER_COMPILED);

    vertShaderModule_ = createShaderModule(vertShaderCode);
    fragShaderModule_ = createShaderModule(fragShaderCode);

    VKMON_INFO("Shaders loaded successfully");
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shaderModule;
}

void VulkanRenderer::createGraphicsPipeline() {
    VKMON_INFO("Creating graphics pipeline...");
    
    // Shader stage creation
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule_;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule_;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input (using ModelVertex structure)
    auto bindingDescription = ModelVertex::getBindingDescription();
    auto attributeDescriptions = ModelVertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapChainExtent_.width;
    viewport.height = (float) swapChainExtent_.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent_;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling (disabled for now)
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Depth and stencil state (enable depth testing for 3D)
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Push constant range for model matrix
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    // Pipeline layout with multiple descriptor sets
    VkDescriptorSetLayout descriptorSetLayouts[] = {globalDescriptorSetLayout_, materialDescriptorSetLayout_};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 2; // Global (set 0) + Material (set 1)
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    // Graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    VKMON_DEBUG("Graphics pipeline created successfully");
}

void VulkanRenderer::createFramebuffers() {
    VKMON_DEBUG("Creating framebuffers...");
    
    // First create image views for swap chain images
    swapChainImageViews_.resize(swapChainImages_.size());
    for (size_t i = 0; i < swapChainImages_.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages_[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat_;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device_, &createInfo, nullptr, &swapChainImageViews_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view!");
        }
    }

    // Create framebuffers with depth buffer
    swapChainFramebuffers_.resize(swapChainImageViews_.size());
    for (size_t i = 0; i < swapChainImageViews_.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            swapChainImageViews_[i],
            depthImageView_
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass_;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent_.width;
        framebufferInfo.height = swapChainExtent_.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &swapChainFramebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }

    VKMON_DEBUG("Framebuffers created successfully");
}

void VulkanRenderer::createCommandPool() {
    VKMON_DEBUG("Creating command pool...");
    
    int graphicsQueueFamily = findGraphicsQueueFamily();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsQueueFamily;

    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }

    VKMON_INFO("Command pool created successfully");
}

void VulkanRenderer::createCommandBuffers() {
    VKMON_INFO("Creating command buffers...");
    
    commandBuffers_.resize(swapChainFramebuffers_.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers_.size();

    if (vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }

    VKMON_INFO("Command buffers allocated successfully");
}

void VulkanRenderer::createSyncObjects() {
    VKMON_INFO("Creating synchronization objects...");
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphore_) != VK_SUCCESS ||
        vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphore_) != VK_SUCCESS ||
        vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFence_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sync objects!");
    }

    VKMON_INFO("Sync objects created successfully");
}

// =============================================================================
// Resource Creation Methods (to be extracted from main.cpp)
// =============================================================================

void VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                                VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device_, buffer, bufferMemory, 0);
}

void VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, 
                               VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
                               VkImage& image, VkDeviceMemory& imageMemory) {
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

    if (vkCreateImage(device_, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory!");
    }

    vkBindImageMemory(device_, image, imageMemory, 0);
}

void VulkanRenderer::createDescriptorSetLayout() {
    // Legacy method - will be removed after refactor
    // For now, maintain both old and new approaches during transition
    createGlobalDescriptorSetLayout();
    createMaterialDescriptorSetLayout();

    // Keep old single descriptor set layout for backward compatibility during transition
    VKMON_INFO("Creating legacy descriptor set layout...");

    // UBO binding (binding 0)
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Texture sampler binding (binding 1)
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Lighting uniform binding (binding 2)
    VkDescriptorSetLayoutBinding lightingLayoutBinding{};
    lightingLayoutBinding.binding = 2;
    lightingLayoutBinding.descriptorCount = 1;
    lightingLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightingLayoutBinding.pImmutableSamplers = nullptr;
    lightingLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // Material uniform binding (binding 3)
    VkDescriptorSetLayoutBinding materialLayoutBinding{};
    materialLayoutBinding.binding = 3;
    materialLayoutBinding.descriptorCount = 1;
    materialLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    materialLayoutBinding.pImmutableSamplers = nullptr;
    materialLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 4> bindings = {uboLayoutBinding, samplerLayoutBinding, lightingLayoutBinding, materialLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create legacy descriptor set layout!");
    }

    VKMON_INFO("Legacy descriptor set layout created successfully");
}

void VulkanRenderer::createGlobalDescriptorSetLayout() {
    VKMON_INFO("Creating global descriptor set layout (UBO, texture, lighting)...");

    // UBO binding (binding 0)
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Texture sampler binding (binding 1)
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Lighting uniform binding (binding 2)
    VkDescriptorSetLayoutBinding lightingLayoutBinding{};
    lightingLayoutBinding.binding = 2;
    lightingLayoutBinding.descriptorCount = 1;
    lightingLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightingLayoutBinding.pImmutableSamplers = nullptr;
    lightingLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {uboLayoutBinding, samplerLayoutBinding, lightingLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &globalDescriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create global descriptor set layout!");
    }

    VKMON_INFO("Global descriptor set layout created successfully");
}

void VulkanRenderer::createMaterialDescriptorSetLayout() {
    VKMON_INFO("Creating material descriptor set layout (per-material data)...");

    // Material uniform binding (binding 0 - rebased from binding 3)
    VkDescriptorSetLayoutBinding materialLayoutBinding{};
    materialLayoutBinding.binding = 0;
    materialLayoutBinding.descriptorCount = 1;
    materialLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    materialLayoutBinding.pImmutableSamplers = nullptr;
    materialLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 1> bindings = {materialLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &materialDescriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create material descriptor set layout!");
    }

    VKMON_INFO("Material descriptor set layout created successfully");
}

void VulkanRenderer::createTextureImage() {
    VKMON_DEBUG("Creating texture image...");
    
    // Create a simple 4x4 checkered texture programmatically
    const int texWidth = 4;
    const int texHeight = 4;
    const int texChannels = 4; // RGBA
    
    // Create a simple checkered pattern
    unsigned char pixels[texWidth * texHeight * texChannels];
    for (int y = 0; y < texHeight; y++) {
        for (int x = 0; x < texWidth; x++) {
            int index = (y * texWidth + x) * texChannels;
            bool isWhite = (x + y) % 2 == 0;
            
            if (isWhite) {
                pixels[index + 0] = 255; // R
                pixels[index + 1] = 255; // G  
                pixels[index + 2] = 255; // B
                pixels[index + 3] = 255; // A
            } else {
                pixels[index + 0] = 0;   // R
                pixels[index + 1] = 0;   // G
                pixels[index + 2] = 0;   // B
                pixels[index + 3] = 255; // A
            }
        }
    }
    
    VkDeviceSize imageSize = texWidth * texHeight * texChannels;

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                stagingBuffer, stagingBufferMemory);

    // Copy pixel data to staging buffer
    void* data;
    vkMapMemory(device_, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device_, stagingBufferMemory);

    // Create image - simplified for now, will add transitions later
    createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, 
               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage_, textureImageMemory_);

    // TODO: Add image transitions and buffer copy when we implement helper methods
    // For now, cleanup staging buffer
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingBufferMemory, nullptr);

    VKMON_DEBUG("Texture image created successfully (basic version)");
}

void VulkanRenderer::createTextureImageView() {
    VKMON_DEBUG("Creating texture image view...");
    textureImageView_ = createImageView(textureImage_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
    VKMON_DEBUG("Texture image view created successfully");
}

void VulkanRenderer::createTextureSampler() {
    VKMON_DEBUG("Creating texture sampler...");
    
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;  // Sharp pixelated look for our checkered pattern
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device_, &samplerInfo, nullptr, &textureSampler_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler!");
    }

    VKMON_INFO("Texture sampler created successfully");
}

void VulkanRenderer::createDescriptorPool() {
    VKMON_INFO("Creating descriptor pool...");
    
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    // UBO descriptors (main UBO + lighting UBO + material UBO)
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 3;
    // Texture sampler descriptor
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }

    VKMON_INFO("Descriptor pool created successfully");
}

void VulkanRenderer::createDescriptorSet() {
    VKMON_INFO("Creating descriptor set...");
    
    VkDescriptorSetLayout layouts[] = {descriptorSetLayout_};
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = layouts;

    if (vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set!");
    }

    // UBO descriptor write (binding 0)
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer_;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    // Texture descriptor write (binding 1)
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureImageView_;
    imageInfo.sampler = textureSampler_;
    
    // Lighting descriptor write (binding 2)
    VkDescriptorBufferInfo lightingBufferInfo{};
    lightingBufferInfo.buffer = lightingSystem_->getLightingBuffer();
    lightingBufferInfo.offset = 0;
    lightingBufferInfo.range = VK_WHOLE_SIZE;
    
    // Material descriptor write (binding 3)
    VkDescriptorBufferInfo materialBufferInfo{};
    materialBufferInfo.buffer = materialBuffer_;
    materialBufferInfo.offset = 0;
    materialBufferInfo.range = sizeof(MaterialData);

    std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

    // UBO descriptor write
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet_;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    // Texture descriptor write
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet_;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;
    
    // Lighting descriptor write
    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = descriptorSet_;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &lightingBufferInfo;
    
    // Material descriptor write
    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = descriptorSet_;
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &materialBufferInfo;

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    VKMON_INFO("Descriptor set created successfully");
}

void VulkanRenderer::createGlobalDescriptorPool() {
    VKMON_INFO("Creating global descriptor pool (UBO, texture, lighting)...");

    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    // UBO and lighting uniform buffer descriptors
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2; // UBO + lighting
    // Texture sampler descriptor
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1; // Only need one global descriptor set

    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &globalDescriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create global descriptor pool!");
    }

    VKMON_INFO("Global descriptor pool created successfully");
}

void VulkanRenderer::createGlobalDescriptorSet() {
    VKMON_INFO("Creating global descriptor set (UBO, texture, lighting)...");

    VkDescriptorSetLayout layouts[] = {globalDescriptorSetLayout_};
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = globalDescriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = layouts;

    if (vkAllocateDescriptorSets(device_, &allocInfo, &globalDescriptorSet_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate global descriptor set!");
    }

    // UBO descriptor write (binding 0)
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer_;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    // Texture descriptor write (binding 1)
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureImageView_;
    imageInfo.sampler = textureSampler_;

    // Lighting descriptor write (binding 2)
    VkDescriptorBufferInfo lightingBufferInfo{};
    lightingBufferInfo.buffer = lightingSystem_->getLightingBuffer();
    lightingBufferInfo.offset = 0;
    lightingBufferInfo.range = VK_WHOLE_SIZE;

    std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

    // UBO descriptor write
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = globalDescriptorSet_;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    // Texture descriptor write
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = globalDescriptorSet_;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    // Lighting descriptor write
    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = globalDescriptorSet_;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &lightingBufferInfo;

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    VKMON_INFO("Global descriptor set created successfully");
}

void VulkanRenderer::createUniformBuffer() {
    VKMON_DEBUG("Creating uniform buffer...");
    
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniformBuffer_, uniformBufferMemory_);

    VKMON_DEBUG("Uniform buffer created successfully");
}

void VulkanRenderer::createMaterialBuffer() {
    VKMON_DEBUG("Creating material buffer...");
    
    VkDeviceSize bufferSize = sizeof(MaterialData);

    createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                materialBuffer_, materialBufferMemory_);
    
    // Initialize with default material
    currentMaterialData_.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 0.0f);
    currentMaterialData_.diffuse = glm::vec4(0.8f, 0.6f, 0.4f, 0.0f);   // Warm brown
    currentMaterialData_.specular = glm::vec4(0.3f, 0.3f, 0.3f, 0.0f);
    currentMaterialData_.shininess = 32.0f;
    
    updateMaterialBuffer();

    VKMON_DEBUG("Material buffer created successfully");
}

void VulkanRenderer::createDepthResources() {
    VKMON_INFO("Creating depth resources...");
    
    VkFormat depthFormat = findDepthFormat();

    createImage(swapChainExtent_.width, swapChainExtent_.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               depthImage_, depthImageMemory_);

    depthImageView_ = createImageView(depthImage_, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    VKMON_INFO("Depth buffer created successfully");
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device_, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image view!");
    }

    return imageView;
}

// =============================================================================
// Rendering Methods (to be extracted from main.cpp)
// =============================================================================

void VulkanRenderer::drawFrame() {
    // Frame setup - uniform updates
    updateUniformBuffer();

    // Use ECS rendering pipeline if callback is registered
    if (ecsRenderCallback_) {
        // Begin ECS frame (handles wait, acquire, command recording start)
        beginECSFrame();

        // Call ECS render callback to submit render commands
        ecsRenderCallback_(*this);

        // End ECS frame (handles command recording end, submit, present)
        endECSFrame();
    } else {
        // Legacy fallback: use static command buffers (for backward compatibility)
        vkWaitForFences(device_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
        vkResetFences(device_, 1, &inFlightFence_);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(device_, swapChain_, UINT64_MAX, imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphore_};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers_[imageIndex];

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphore_};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFence_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapChain_};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(graphicsQueue_, &presentInfo);
    }
}

void VulkanRenderer::recordCommandBuffers() {
    VKMON_INFO("Recording command buffers...");
    
    for (size_t i = 0; i < commandBuffers_.size(); i++) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffers_[i], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass_;
        renderPassInfo.framebuffer = swapChainFramebuffers_[i];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent_;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};
        
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffers_[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);
        
        // Legacy rendering path - bind global descriptor set (set 0)
        vkCmdBindDescriptorSets(commandBuffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &globalDescriptorSet_, 0, nullptr);

        // Legacy rendering path - bind default material descriptor set (set 1)
        if (materialSystem_ && materialSystem_->getMaterialCount() > 0) {
            VkDescriptorSet defaultMaterialDescriptorSet = materialSystem_->getDescriptorSet(0); // Use first material as default
            if (defaultMaterialDescriptorSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(commandBuffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 1, 1, &defaultMaterialDescriptorSet, 0, nullptr);
            }
        }
        
        // Render loaded 3D model (if available)
        if (currentModel_ && !currentModel_->meshes.empty()) {
            for (const auto& mesh : currentModel_->meshes) {
                if (mesh->vertexBuffer && mesh->indexBuffer) {
                    VkBuffer vertexBuffers[] = {mesh->vertexBuffer->getBuffer()};
                    VkDeviceSize offsets[] = {0};
                    vkCmdBindVertexBuffers(commandBuffers_[i], 0, 1, vertexBuffers, offsets);
                    vkCmdBindIndexBuffer(commandBuffers_[i], mesh->indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
                    
                    vkCmdDrawIndexed(commandBuffers_[i], static_cast<uint32_t>(mesh->indices.size()), 1, 0, 0, 0);
                }
            }
        }

        // Render ImGui if enabled
        if (imguiEnabled_ && imguiInitialized_) {
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffers_[i]);
        }

        vkCmdEndRenderPass(commandBuffers_[i]);

        if (vkEndCommandBuffer(commandBuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }

    VKMON_INFO("Drawing commands recorded successfully");
}

void VulkanRenderer::updateUniformBuffer() {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    // Camera constants
    constexpr float CAMERA_FOV = 45.0f;
    constexpr float NEAR_PLANE = 0.1f;
    constexpr float FAR_PLANE = 10.0f;

    UniformBufferObject ubo{};

    // View matrix: use dynamic camera position from WASD input
    ubo.view = camera_->getViewMatrix();

    // Projection matrix: perspective projection
    ubo.proj = glm::perspective(glm::radians(CAMERA_FOV), swapChainExtent_.width / (float) swapChainExtent_.height, NEAR_PLANE, FAR_PLANE);

    // GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
    ubo.proj[1][1] *= -1;

    // Camera position for specular lighting calculations
    ubo.cameraPos = camera_->position;
    ubo._padding = 0.0f;

    void* data;
    vkMapMemory(device_, uniformBufferMemory_, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(device_, uniformBufferMemory_);
}

void VulkanRenderer::updateMaterialBuffer() {
    void* data;
    vkMapMemory(device_, materialBufferMemory_, 0, sizeof(MaterialData), 0, &data);
    memcpy(data, &currentMaterialData_, sizeof(MaterialData));
    vkUnmapMemory(device_, materialBufferMemory_);
}

// =============================================================================
// Helper Methods (to be extracted from main.cpp)
// =============================================================================

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

VkFormat VulkanRenderer::findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkFormat VulkanRenderer::findSupportedFormat(const std::vector<VkFormat>& candidates, 
                                            VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported depth format!");
}

bool VulkanRenderer::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

// =============================================================================
// Temporary System Initialization (will be moved to VulkanContext)
// =============================================================================

void VulkanRenderer::initializeCoreSystemsTemporary() {
    VKMON_INFO("Initializing core engine systems...");
    
    // This is a temporary solution until we create VulkanContext
    // These systems need Vulkan device which we don't have until after createLogicalDevice()
    
    resourceManager_ = std::make_shared<ResourceManager>(device_, physicalDevice_);
    // Create robust path to assets directory based on executable location
    std::filesystem::path executablePath = std::filesystem::current_path();
    std::filesystem::path assetsPath;
    
    // Try multiple possible asset locations
    std::vector<std::filesystem::path> possiblePaths = {
        executablePath / "assets",                    // Same directory as executable
        executablePath / ".." / ".." / "assets",      // From build/Debug/ to project root
        executablePath / ".." / "assets",             // From build/ to project root
        executablePath.parent_path() / "assets",      // Parent directory
        executablePath.parent_path().parent_path() / "assets"  // Grandparent directory
    };
    
    bool foundAssets = false;
    for (const auto& path : possiblePaths) {
        std::filesystem::path absolutePath = std::filesystem::absolute(path);
        VKMON_DEBUG("Checking assets path: " + absolutePath.string());
        
        if (std::filesystem::exists(absolutePath) && std::filesystem::is_directory(absolutePath)) {
            // Verify it contains expected subdirectories
            if (std::filesystem::exists(absolutePath / "models")) {
                assetsPath = absolutePath;
                foundAssets = true;
                VKMON_INFO("Found assets directory: " + assetsPath.string());
                break;
            }
        }
    }
    
    if (!foundAssets) {
        VKMON_ERROR("Could not find assets directory in any expected location");
        VKMON_ERROR("Current executable path: " + executablePath.string());
        throw std::runtime_error("Assets directory not found - please ensure assets/ exists relative to executable");
    }
    
    assetManager_ = std::make_shared<AssetManager>(device_, physicalDevice_, commandPool_, graphicsQueue_, assetsPath.string() + "/");
    modelLoader_ = std::make_shared<ModelLoader>(resourceManager_, assetManager_);
    lightingSystem_ = std::make_shared<LightingSystem>(resourceManager_);
    lightingSystem_->createLightingBuffers();
    materialSystem_ = std::make_shared<MaterialSystem>(resourceManager_);
    materialSystem_->setDescriptorSetLayout(materialDescriptorSetLayout_);
    materialSystem_->createMaterialBuffers();

    // Create a default material for legacy rendering compatibility
    MaterialData defaultMaterial;
    materialSystem_->createMaterial(defaultMaterial);

    VKMON_INFO("Core engine systems initialized successfully!");
}

void VulkanRenderer::loadTestModel() {
    VKMON_INFO("Loading test model...");
    
    // Load the test cube
    currentModel_ = modelLoader_->loadModel("test_cube.obj");
    VKMON_INFO("Test cube model loaded successfully!");
    
    // Create a default material for the model (simplified version)
    currentMaterialData_.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 0.0f);
    currentMaterialData_.diffuse = glm::vec4(0.8f, 0.8f, 0.8f, 0.0f);
    currentMaterialData_.specular = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    currentMaterialData_.shininess = 32.0f;
}

void VulkanRenderer::cycleMaterialPreset() {
    if (!materialSystem_) {
        VKMON_WARNING("Cannot cycle materials - MaterialSystem not available");
        return;
    }

    // Ensure we have the standard material presets in MaterialSystem
    ensureStandardMaterialsExist();

    // Cycle to next material preset
    currentMaterialPreset_ = (currentMaterialPreset_ + 1) % materialSystem_->getMaterialCount();

    const char* materialNames[] = {"Default", "Gold", "Ruby", "Chrome", "Emerald"};
    const char* materialName = (currentMaterialPreset_ < 5) ? materialNames[currentMaterialPreset_] : "Unknown";

    VKMON_INFO("[MATERIAL] Cycled to preset: " + std::string(materialName) +
               " (" + std::to_string(currentMaterialPreset_ + 1) + "/" +
               std::to_string(materialSystem_->getMaterialCount()) + ")");

    // For now, this provides user feedback that material cycling is working
    // In the future, this could update specific entities in the ECS system
}

void VulkanRenderer::ensureStandardMaterialsExist() {
    // Ensure we have at least 5 standard material presets
    if (materialSystem_->getMaterialCount() < 5) {
        // Define standard material presets
        std::vector<MaterialData> standardMaterials = {
            // 0: Default (already created in initializeCoreEngineSystems)
            MaterialData(glm::vec3(0.1f, 0.1f, 0.1f), glm::vec3(0.8f, 0.8f, 0.8f), glm::vec3(1.0f, 1.0f, 1.0f), 32.0f),
            // 1: Gold
            MaterialData(glm::vec3(0.24725f, 0.1995f, 0.0745f), glm::vec3(0.75164f, 0.60648f, 0.22648f), glm::vec3(0.628281f, 0.555802f, 0.366065f), 51.2f),
            // 2: Ruby
            MaterialData(glm::vec3(0.1745f, 0.01175f, 0.01175f), glm::vec3(0.61424f, 0.04136f, 0.04136f), glm::vec3(0.727811f, 0.626959f, 0.626959f), 76.8f),
            // 3: Chrome
            MaterialData(glm::vec3(0.25f, 0.25f, 0.25f), glm::vec3(0.4f, 0.4f, 0.4f), glm::vec3(0.774597f, 0.774597f, 0.774597f), 76.8f),
            // 4: Emerald
            MaterialData(glm::vec3(0.0215f, 0.1745f, 0.0215f), glm::vec3(0.07568f, 0.61424f, 0.07568f), glm::vec3(0.633f, 0.727811f, 0.633f), 76.8f)
        };

        // Create materials starting from index 1 (0 is already created)
        for (size_t i = materialSystem_->getMaterialCount(); i < standardMaterials.size(); ++i) {
            materialSystem_->createMaterial(standardMaterials[i]);
        }

        VKMON_INFO("Created " + std::to_string(standardMaterials.size()) + " standard material presets");
    }
}

// =============================================================================
// Cleanup Methods
// =============================================================================

void VulkanRenderer::cleanup() {
    VKMON_INFO("Beginning VulkanRenderer cleanup...");
    
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    
    cleanupSwapChain();
    
    // Cleanup sync objects
    if (imageAvailableSemaphore_ != VK_NULL_HANDLE) {
        vkDestroySemaphore(device_, imageAvailableSemaphore_, nullptr);
        imageAvailableSemaphore_ = VK_NULL_HANDLE;
    }
    if (renderFinishedSemaphore_ != VK_NULL_HANDLE) {
        vkDestroySemaphore(device_, renderFinishedSemaphore_, nullptr);
        renderFinishedSemaphore_ = VK_NULL_HANDLE;
    }
    if (inFlightFence_ != VK_NULL_HANDLE) {
        vkDestroyFence(device_, inFlightFence_, nullptr);
        inFlightFence_ = VK_NULL_HANDLE;
    }
    
    // Cleanup command pool
    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }
    
    // Cleanup descriptor resources
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }

    // Cleanup global descriptor resources
    if (globalDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, globalDescriptorPool_, nullptr);
        globalDescriptorPool_ = VK_NULL_HANDLE;
    }
    if (globalDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, globalDescriptorSetLayout_, nullptr);
        globalDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
    if (materialDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, materialDescriptorSetLayout_, nullptr);
        materialDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
    
    // Cleanup texture resources
    if (textureSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, textureSampler_, nullptr);
        textureSampler_ = VK_NULL_HANDLE;
    }
    if (textureImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, textureImageView_, nullptr);
        textureImageView_ = VK_NULL_HANDLE;
    }
    if (textureImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, textureImage_, nullptr);
        textureImage_ = VK_NULL_HANDLE;
    }
    if (textureImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, textureImageMemory_, nullptr);
        textureImageMemory_ = VK_NULL_HANDLE;
    }
    
    // Cleanup buffers
    if (uniformBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, uniformBuffer_, nullptr);
        uniformBuffer_ = VK_NULL_HANDLE;
    }
    if (uniformBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, uniformBufferMemory_, nullptr);
        uniformBufferMemory_ = VK_NULL_HANDLE;
    }
    if (materialBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, materialBuffer_, nullptr);
        materialBuffer_ = VK_NULL_HANDLE;
    }
    if (materialBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, materialBufferMemory_, nullptr);
        materialBufferMemory_ = VK_NULL_HANDLE;
    }
    
    // Cleanup Vulkan context
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    
    initialized_ = false;
    VKMON_INFO("VulkanRenderer cleanup completed");
}

void VulkanRenderer::cleanupSwapChain() {
    // Cleanup depth buffer
    if (depthImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, depthImageView_, nullptr);
        depthImageView_ = VK_NULL_HANDLE;
    }
    if (depthImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, depthImage_, nullptr);
        depthImage_ = VK_NULL_HANDLE;
    }
    if (depthImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, depthImageMemory_, nullptr);
        depthImageMemory_ = VK_NULL_HANDLE;
    }
    
    // Cleanup framebuffers
    for (auto framebuffer : swapChainFramebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
    }
    swapChainFramebuffers_.clear();
    
    // Cleanup pipeline
    if (graphicsPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, graphicsPipeline_, nullptr);
        graphicsPipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    
    // Cleanup render pass
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
    
    // Cleanup shader modules
    if (vertShaderModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, vertShaderModule_, nullptr);
        vertShaderModule_ = VK_NULL_HANDLE;
    }
    if (fragShaderModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, fragShaderModule_, nullptr);
        fragShaderModule_ = VK_NULL_HANDLE;
    }
    
    // Cleanup image views
    for (auto imageView : swapChainImageViews_) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, imageView, nullptr);
        }
    }
    swapChainImageViews_.clear();
    
    // Cleanup swapchain
    if (swapChain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapChain_, nullptr);
        swapChain_ = VK_NULL_HANDLE;
    }
    
    swapChainImages_.clear();
}

// =============================================================================
// Debug and Logging Helpers
// =============================================================================

void VulkanRenderer::logRenderingState(const std::string& operation) const {
    VKMON_INFO("[RENDERING] " + operation);
    VKMON_INFO("  Frame time: " + std::to_string(lastFrameTime_) + "ms");
    VKMON_INFO("  Initialized: " + std::string(initialized_ ? "true" : "false"));
    if (currentModel_) {
        VKMON_INFO("  Current model: loaded");
    } else {
        VKMON_INFO("  Current model: none");
    }
}

// =============================================================================
// ImGui Debug Interface Integration - Phase 6.3
// =============================================================================

void VulkanRenderer::initializeImGui() {
    if (imguiInitialized_) {
        VKMON_WARNING("ImGui already initialized");
        return;
    }

    VKMON_INFO("Initializing ImGui debug interface...");

    // Create ImGui descriptor pool
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
    poolInfo.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &imguiDescriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool");
    }

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup ImGui style (dark theme)
    ImGui::StyleColorsDark();

    // Get GLFW window handle from Window class
    GLFWwindow* glfwWindow = window_->getWindow();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForVulkan(glfwWindow, true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance_;
    initInfo.PhysicalDevice = physicalDevice_;
    initInfo.Device = device_;
    initInfo.QueueFamily = findGraphicsQueueFamily();
    initInfo.Queue = graphicsQueue_;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = imguiDescriptorPool_;
    initInfo.RenderPass = renderPass_;
    initInfo.Subpass = 0;
    initInfo.Allocator = nullptr;
    initInfo.MinImageCount = static_cast<uint32_t>(swapChainImages_.size());
    initInfo.ImageCount = static_cast<uint32_t>(swapChainImages_.size());
    initInfo.CheckVkResultFn = nullptr;

    ImGui_ImplVulkan_Init(&initInfo);

    // Upload ImGui fonts (newer ImGui handles this automatically)
    ImGui_ImplVulkan_CreateFontsTexture();

    imguiInitialized_ = true;
    VKMON_INFO("ImGui debug interface initialized successfully");
}

void VulkanRenderer::cleanupImGui() {
    if (!imguiInitialized_) return;

    VKMON_INFO("Cleaning up ImGui debug interface...");

    vkDeviceWaitIdle(device_);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (imguiDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, imguiDescriptorPool_, nullptr);
        imguiDescriptorPool_ = VK_NULL_HANDLE;
    }

    imguiInitialized_ = false;
    VKMON_INFO("ImGui debug interface cleanup complete");
}

void VulkanRenderer::beginImGuiFrame() {
    if (!imguiInitialized_ || !imguiEnabled_) return;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void VulkanRenderer::endImGuiFrame() {
    if (!imguiInitialized_ || !imguiEnabled_) return;

    ImGui::Render();

    // The ImGui draw data will be recorded to the command buffer
    // in the existing command buffer recording (renderFrame method)
}

