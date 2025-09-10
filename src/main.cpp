/*
 * VulkanMon - 3D Graphics Engine
 * 
 * Complete 3D Vulkan renderer with advanced core engine systems.
 * Production-ready foundation for the VulkanMon creature collector game engine.
 * 
 * Architecture:
 * - ResourceManager: RAII wrappers for VkBuffer, VkImage, memory management
 * - Logger: Thread-safe logging with console/file output and performance tracking
 * - AssetManager: Texture loading, caching, asset discovery and validation
 * - ModelLoader: Assimp-based 3D model loading with material and texture support
 * - LightingSystem: Directional and ambient lighting with descriptor management
 * - Camera: Interactive WASD movement with mouse look
 */

#include <iostream>
#include <vector>
#include <fstream>
#include <array>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#include "Camera.h"
#include "Utils.h"
#include "Logger.h"
#include "ResourceManager.h" 
#include "AssetManager.h"
#include "LightingSystem.h"
#include "MaterialSystem.h"
#include "ModelLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace VulkanMon;

// Configuration constants - keeping it simple for now
constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 600;
constexpr float CAMERA_SPEED = 2.5f;
constexpr float CAMERA_FOV = 45.0f;
constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 10.0f;
constexpr const char* VERTEX_SHADER_COMPILED = "shaders/vert.spv";
constexpr const char* FRAGMENT_SHADER_COMPILED = "shaders/frag.spv";


struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 cameraPos;
    float _padding;
};


class HelloTriangleApp {
public:
    // Static key callback for hot shader reloading and lighting controls
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        HelloTriangleApp* app = reinterpret_cast<HelloTriangleApp*>(glfwGetWindowUserPointer(window));
        
        if (action == GLFW_PRESS) {
            switch (key) {
                case GLFW_KEY_R:
                    std::cout << "\n[HOT RELOAD] R key pressed - reloading shaders..." << std::endl;
                    app->reloadShaders();
                    break;
                    
                case GLFW_KEY_1:
                    // Adjust directional light intensity
                    app->adjustDirectionalLightIntensity(0.1f);
                    break;
                    
                case GLFW_KEY_2:
                    app->adjustDirectionalLightIntensity(-0.1f);
                    break;
                    
                case GLFW_KEY_3:
                    // Cycle through preset lighting configurations
                    app->cycleLightingPreset();
                    break;
                    
                case GLFW_KEY_4:
                    // Toggle ambient lighting
                    app->toggleAmbientLighting();
                    break;
                    
                case GLFW_KEY_L:
                    // Print lighting debug info
                    app->printLightingInfo();
                    break;
                    
                case GLFW_KEY_M:
                    // Cycle through material presets
                    app->cycleMaterialPreset();
                    break;
                    
                case GLFW_KEY_5:
                    // Increase material shininess
                    app->adjustMaterialShininess(10.0f);
                    break;
                    
                case GLFW_KEY_6:
                    // Decrease material shininess
                    app->adjustMaterialShininess(-10.0f);
                    break;
            }
        }
    }

    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    VkRenderPass renderPass;
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
    
    // Texture resources
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    
    // Uniform Buffer Object resources
    VkBuffer uniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory uniformBufferMemory = VK_NULL_HANDLE;
    VkBuffer materialBuffer = VK_NULL_HANDLE;
    VkDeviceMemory materialBufferMemory = VK_NULL_HANDLE;
    
    // Depth buffer resources
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;
    
    // Camera system for WASD movement (Phase 2.5)
    Camera camera;
    
    // Core Engine Systems
    std::shared_ptr<ResourceManager> resourceManager;
    std::shared_ptr<AssetManager> assetManager;
    std::shared_ptr<ModelLoader> modelLoader;
    std::unique_ptr<Model> currentModel;
    
    // Lighting System
    std::shared_ptr<LightingSystem> lightingSystem;
    std::shared_ptr<MaterialSystem> materialSystem;

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        
        window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "VulkanMon", nullptr, nullptr);
        
        // Set up key callback
        glfwSetWindowUserPointer(window, this);
        glfwSetKeyCallback(window, keyCallback);
    }

    void initVulkan() {
        Logger::getInstance().enableConsoleOutput(true);
        Logger::getInstance().setLogLevel(LogLevel::INFO_LEVEL);
        VKMON_INFO("VulkanMon - Initializing Core Engine Systems");
        
        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createRenderPass();
        createDescriptorSetLayout();
        createShaderModules();
        createGraphicsPipeline();
        createDepthResources();
        createFramebuffers();
        createCommandPool();
        
        // Initialize core systems (after command pool creation)
        initCoreEngineSystems();
        
        // Load 3D model with new model system
        loadTestModel();
        
        createUniformBuffer();
        createMaterialBuffer();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        createDescriptorPool();
        createDescriptorSet();
        createCommandBuffers();
        recordCommandBuffers();
        createSyncObjects();
    }

    void createInstance() {
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

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance!");
        }

        std::cout << "Vulkan instance created successfully!\n";
    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface!");
        }
        std::cout << "Window surface created successfully!\n";
    }

    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // Simple selection: just pick the first device
        physicalDevice = devices[0];

        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        std::cout << "Selected GPU: " << deviceProperties.deviceName << "\n";
    }

    int findGraphicsQueueFamily() {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        // Find a queue family that supports graphics
        for (int i = 0; i < queueFamilies.size(); i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                return i;
            }
        }
        return -1;
    }

    void createLogicalDevice() {
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

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device!");
        }

        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
        std::cout << "Logical device created successfully!\n";
    }

    void createSwapChain() {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

        // Choose swap surface format (prefer SRGB if available)
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

        VkSurfaceFormatKHR surfaceFormat = formats[0];
        for (const auto& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surfaceFormat = format;
                break;
            }
        }

        // Choose extent
        if (capabilities.currentExtent.width != UINT32_MAX) {
            swapChainExtent = capabilities.currentExtent;
        } else {
            swapChainExtent = {WINDOW_WIDTH, WINDOW_HEIGHT};
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = swapChainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // V-Sync
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swap chain!");
        }

        // Get swap chain images
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        std::cout << "Swap chain created successfully with " << imageCount << " images!\n";
    }

    void createRenderPass() {
        // Color attachment (what we render to)
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChainImageFormat;
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

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass!");
        }

        std::cout << "Render pass created successfully!\n";
    }


    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module!");
        }

        return shaderModule;
    }

    void createShaderModules() {
        auto vertShaderCode = Utils::readFile(VERTEX_SHADER_COMPILED);
        auto fragShaderCode = Utils::readFile(FRAGMENT_SHADER_COMPILED);

        vertShaderModule = createShaderModule(vertShaderCode);
        fragShaderModule = createShaderModule(fragShaderCode);

        std::cout << "Shaders loaded successfully!\n";
    }


    void reloadShaders() {
        try {
            // 1. Recompile shader files
            if (!Utils::recompileShaders()) {
                std::cout << "[ERROR] Hot reload failed - keeping current shaders active" << std::endl;
                return;
            }
            
            // 2. Wait for device to be idle
            vkDeviceWaitIdle(device);
            
            // 3. Destroy old pipeline and shader modules
            vkDestroyPipeline(device, graphicsPipeline, nullptr);
            vkDestroyShaderModule(device, vertShaderModule, nullptr);
            vkDestroyShaderModule(device, fragShaderModule, nullptr);
            
            // 4. Recreate shader modules and pipeline
            createShaderModules();
            createGraphicsPipeline();
            
            std::cout << "[SUCCESS] Hot reload complete - new shaders active!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[ERROR] Hot reload failed: " << e.what() << std::endl;
            std::cout << "[WARN] Application continuing with previous shaders" << std::endl;
        }
    }

    void createGraphicsPipeline() {
        // Shader stage creation
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
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
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;

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

        // Pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
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
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        std::cout << "Graphics pipeline created successfully!\n";
    }

    void createFramebuffers() {
        // First create image views for swap chain images
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image view!");
            }
        }

        // Create framebuffers with depth buffer
        swapChainFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            std::array<VkImageView, 2> attachments = {
                swapChainImageViews[i],
                depthImageView
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }

        std::cout << "Framebuffers created successfully!\n";
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    }


    void createCommandPool() {
        int graphicsQueueFamily = findGraphicsQueueFamily();

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }

        std::cout << "Command pool created successfully!\n";
    }

    void createCommandBuffers() {
        commandBuffers.resize(swapChainFramebuffers.size());

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }

        std::cout << "Command buffers allocated successfully!\n";
    }

    void recordCommandBuffers() {
        for (size_t i = 0; i < commandBuffers.size(); i++) {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("Failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = renderPass;
            renderPassInfo.framebuffer = swapChainFramebuffers[i];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = swapChainExtent;

            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearValues[1].depthStencil = {1.0f, 0};
            
            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            
            vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
            
            // Render loaded 3D model
            if (currentModel && !currentModel->meshes.empty()) {
                for (const auto& mesh : currentModel->meshes) {
                    if (mesh->vertexBuffer && mesh->indexBuffer) {
                        VkBuffer vertexBuffers[] = {mesh->vertexBuffer->getBuffer()};
                        VkDeviceSize offsets[] = {0};
                        vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
                        vkCmdBindIndexBuffer(commandBuffers[i], mesh->indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
                        
                        vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(mesh->indices.size()), 1, 0, 0, 0);
                    }
                }
            }
            vkCmdEndRenderPass(commandBuffers[i]);

            if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to record command buffer!");
            }
        }

        std::cout << "Drawing commands recorded successfully!\n";
    }

    void createSyncObjects() {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sync objects!");
        }

        std::cout << "Sync objects created successfully!\n";
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(device);
    }

    void drawFrame() {
        vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFence);

        handleCameraInput();
        updateUniformBuffer();

        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(graphicsQueue, &presentInfo);
    }

    // Vulkan helper functions
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
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

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate image memory!");
        }

        vkBindImageMemory(device, image, imageMemory, 0);
    }

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT) {
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
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture image view!");
        }

        return imageView;
    }

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::invalid_argument("Unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        endSingleTimeCommands(commandBuffer);
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

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

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(commandBuffer);
    }

    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void createDescriptorSetLayout() {
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

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }

        std::cout << "Descriptor set layout created successfully!" << std::endl;
    }

    void createTextureImage() {
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
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        // Copy pixel data to staging buffer
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        // Create image
        createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

        // Transition image layout and copy from staging buffer
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Cleanup staging buffer
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);

        std::cout << "Texture image created successfully!" << std::endl;
    }

    void createTextureImageView() {
        textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB);
        std::cout << "Texture image view created successfully!" << std::endl;
    }

    void createTextureSampler() {
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

        if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture sampler!");
        }

        std::cout << "Texture sampler created successfully!" << std::endl;
    }

    void createDescriptorPool() {
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

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }

        std::cout << "Descriptor pool created successfully!" << std::endl;
    }

    void createDescriptorSet() {
        VkDescriptorSetLayout layouts[] = {descriptorSetLayout};
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = layouts;

        if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor set!");
        }

        // UBO descriptor write (binding 0)
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        // Texture descriptor write (binding 1)
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImageView;
        imageInfo.sampler = textureSampler;
        
        // Lighting descriptor write (binding 2)
        VkDescriptorBufferInfo lightingBufferInfo{};
        lightingBufferInfo.buffer = lightingSystem->getLightingBuffer();
        lightingBufferInfo.offset = 0;
        lightingBufferInfo.range = VK_WHOLE_SIZE;
        
        // Material descriptor write (binding 3)
        VkDescriptorBufferInfo materialBufferInfo{};
        materialBufferInfo.buffer = materialBuffer;
        materialBufferInfo.offset = 0;
        materialBufferInfo.range = sizeof(MaterialData);

        std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

        // UBO descriptor write
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        // Texture descriptor write
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;
        
        // Lighting descriptor write
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = descriptorSet;
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &lightingBufferInfo;
        
        // Material descriptor write
        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = descriptorSet;
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &materialBufferInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        std::cout << "Descriptor set created successfully!" << std::endl;
    }

    void createUniformBuffer() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    uniformBuffer, uniformBufferMemory);

        std::cout << "Uniform buffer created successfully!" << std::endl;
    }
    
    MaterialData currentMaterialData;
    void createMaterialBuffer() {
        VkDeviceSize bufferSize = sizeof(MaterialData);

        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    materialBuffer, materialBufferMemory);
        
        // Initialize with default material
        currentMaterialData.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 0.0f);
        currentMaterialData.diffuse = glm::vec4(0.8f, 0.6f, 0.4f, 0.0f);   // Warm brown
        currentMaterialData.specular = glm::vec4(0.3f, 0.3f, 0.3f, 0.0f);
        currentMaterialData.shininess = 32.0f;
        
        updateMaterialBuffer();

        std::cout << "Material buffer created successfully!" << std::endl;
    }
    
    void updateMaterialBuffer() {
        void* data;
        vkMapMemory(device, materialBufferMemory, 0, sizeof(MaterialData), 0, &data);
        memcpy(data, &currentMaterialData, sizeof(MaterialData));
        vkUnmapMemory(device, materialBufferMemory);
    }

    void handleCameraInput() {
        camera.processInput(window);
    }

    void updateUniformBuffer() {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        
        // Model matrix: 3D rotation around both X and Y axes for full 3D effect
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(CAMERA_FOV), glm::vec3(1.0f, 0.0f, 0.0f));
        ubo.model = glm::rotate(ubo.model, time * glm::radians(60.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        
        // View matrix: use dynamic camera position from WASD input
        ubo.view = camera.getViewMatrix();
        
        // Projection matrix: perspective projection
        ubo.proj = glm::perspective(glm::radians(CAMERA_FOV), swapChainExtent.width / (float) swapChainExtent.height, NEAR_PLANE, FAR_PLANE);
        
        // GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
        ubo.proj[1][1] *= -1;
        
        // Camera position for specular lighting calculations
        ubo.cameraPos = camera.position;
        ubo._padding = 0.0f;

        void* data;
        vkMapMemory(device, uniformBufferMemory, 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(device, uniformBufferMemory);
    }

    VkFormat findDepthFormat() {
        return findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("Failed to find supported depth format!");
    }

    void createDepthResources() {
        VkFormat depthFormat = findDepthFormat();

        createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                   depthImage, depthImageMemory);

        depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

        std::cout << "Depth buffer created successfully!" << std::endl;
    }
    
    // Core Engine System Initialization
    void initCoreEngineSystems() {
        VKMON_INFO("Initializing ResourceManager...");
        resourceManager = std::make_shared<ResourceManager>(device, physicalDevice);
        
        VKMON_INFO("Initializing AssetManager...");
        assetManager = std::make_shared<AssetManager>(device, physicalDevice, commandPool, graphicsQueue);
        
        VKMON_INFO("Initializing ModelLoader...");
        modelLoader = std::make_shared<ModelLoader>(resourceManager, assetManager);
        
        VKMON_INFO("Initializing LightingSystem...");
        lightingSystem = std::make_shared<LightingSystem>(resourceManager);
        lightingSystem->createLightingBuffers();
        
        VKMON_INFO("Initializing MaterialSystem...");
        materialSystem = std::make_shared<MaterialSystem>(resourceManager);
        materialSystem->createMaterialBuffers();
        
        VKMON_INFO("Core engine systems initialized successfully!");
    }
    
    void loadTestModel() {
        VKMON_INFO("Loading test model...");
        
        try {
            // Try to load the test cube we created
            currentModel = modelLoader->loadModel("test_cube.obj");
            VKMON_INFO("Test cube model loaded successfully!");
        } catch (const std::exception& e) {
            VKMON_WARNING("Failed to load test_cube.obj, creating procedural cube: " + std::string(e.what()));
            // Fallback to procedural cube
            currentModel = modelLoader->createTestCube();
            VKMON_INFO("Procedural test cube created successfully!");
        }
        
        // Create a default material for the model
        createDefaultMaterial();
    }
    
    uint32_t currentMaterialId = 0;
    void createDefaultMaterial() {
        MaterialData defaultMaterial;
        defaultMaterial.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 0.0f);
        defaultMaterial.diffuse = glm::vec4(0.8f, 0.6f, 0.4f, 0.0f);   // Warm brown
        defaultMaterial.specular = glm::vec4(0.3f, 0.3f, 0.3f, 0.0f);
        defaultMaterial.shininess = 32.0f;
        
        currentMaterialId = materialSystem->createMaterial(defaultMaterial);
        VKMON_INFO("Created default material with ID: " + std::to_string(currentMaterialId));
    }
    
    // Lighting Control Methods
    void adjustDirectionalLightIntensity(float delta) {
        auto& lighting = lightingSystem->getCurrentLighting();
        lighting.directionalLight.intensity = std::max(0.0f, std::min(3.0f, lighting.directionalLight.intensity + delta));
        lightingSystem->updateLighting(lighting);
        std::cout << "[LIGHTING] Directional light intensity: " << lighting.directionalLight.intensity << std::endl;
    }
    
    int currentLightingPreset = 0;
    void cycleLightingPreset() {
        currentLightingPreset = (currentLightingPreset + 1) % 4;
        auto& lighting = lightingSystem->getCurrentLighting();
        
        switch (currentLightingPreset) {
            case 0: // Default lighting
                lighting.directionalLight.direction = glm::normalize(glm::vec3(0.2f, -1.0f, -0.3f));
                lighting.directionalLight.intensity = 1.2f;
                lighting.directionalLight.color = glm::vec3(1.0f, 1.0f, 1.0f);
                lighting.ambientColor = glm::vec3(0.15f, 0.15f, 0.2f);
                lighting.ambientIntensity = 0.25f;
                std::cout << "[LIGHTING] Preset: Default" << std::endl;
                break;
            case 1: // Bright daylight
                lighting.directionalLight.direction = glm::normalize(glm::vec3(0.3f, -1.0f, -0.2f));
                lighting.directionalLight.intensity = 1.8f;
                lighting.directionalLight.color = glm::vec3(1.0f, 0.95f, 0.8f);
                lighting.ambientColor = glm::vec3(0.3f, 0.3f, 0.4f);
                lighting.ambientIntensity = 0.35f;
                std::cout << "[LIGHTING] Preset: Bright Daylight" << std::endl;
                break;
            case 2: // Warm sunset
                lighting.directionalLight.direction = glm::normalize(glm::vec3(1.0f, -0.6f, 0.2f));
                lighting.directionalLight.intensity = 1.0f;
                lighting.directionalLight.color = glm::vec3(1.0f, 0.6f, 0.3f);
                lighting.ambientColor = glm::vec3(0.25f, 0.15f, 0.1f);
                lighting.ambientIntensity = 0.4f;
                std::cout << "[LIGHTING] Preset: Warm Sunset" << std::endl;
                break;
            case 3: // Cool moonlight
                lighting.directionalLight.direction = glm::normalize(glm::vec3(-0.4f, -1.0f, 0.3f));
                lighting.directionalLight.intensity = 0.6f;
                lighting.directionalLight.color = glm::vec3(0.6f, 0.7f, 1.0f);
                lighting.ambientColor = glm::vec3(0.05f, 0.05f, 0.15f);
                lighting.ambientIntensity = 0.15f;
                std::cout << "[LIGHTING] Preset: Cool Moonlight" << std::endl;
                break;
        }
        lightingSystem->updateLighting(lighting);
    }
    
    void toggleAmbientLighting() {
        auto& lighting = lightingSystem->getCurrentLighting();
        lighting.ambientIntensity = (lighting.ambientIntensity > 0.0f) ? 0.0f : 0.3f;
        lightingSystem->updateLighting(lighting);
        std::cout << "[LIGHTING] Ambient lighting: " << (lighting.ambientIntensity > 0.0f ? "ON" : "OFF") << std::endl;
    }
    
    void printLightingInfo() {
        const auto& lighting = lightingSystem->getCurrentLighting();
        std::cout << "\n[LIGHTING DEBUG INFO]" << std::endl;
        std::cout << "Directional Light:" << std::endl;
        std::cout << "  Direction: (" << lighting.directionalLight.direction.x << ", " 
                  << lighting.directionalLight.direction.y << ", " << lighting.directionalLight.direction.z << ")" << std::endl;
        std::cout << "  Intensity: " << lighting.directionalLight.intensity << std::endl;
        std::cout << "  Color: (" << lighting.directionalLight.color.r << ", " 
                  << lighting.directionalLight.color.g << ", " << lighting.directionalLight.color.b << ")" << std::endl;
        std::cout << "Ambient Light:" << std::endl;
        std::cout << "  Color: (" << lighting.ambientColor.r << ", " 
                  << lighting.ambientColor.g << ", " << lighting.ambientColor.b << ")" << std::endl;
        std::cout << "  Intensity: " << lighting.ambientIntensity << std::endl;
        std::cout << "\nControls:" << std::endl;
        std::cout << "  1/2: Adjust directional light intensity" << std::endl;
        std::cout << "  3: Cycle lighting presets" << std::endl;
        std::cout << "  4: Toggle ambient lighting" << std::endl;
        std::cout << "  5/6: Adjust material shininess" << std::endl;
        std::cout << "  M: Cycle material presets" << std::endl;
        std::cout << "  L: Show this debug info" << std::endl;
        std::cout << "  R: Reload shaders" << std::endl << std::endl;
    }
    
    // Material Control Methods
    
    void adjustMaterialShininess(float delta) {
        float oldShininess = currentMaterialData.shininess;
        currentMaterialData.shininess = std::max(1.0f, std::min(256.0f, currentMaterialData.shininess + delta));
        updateMaterialBuffer(); // Update the simple material buffer
        std::cout << "\n[MATERIAL SHININESS] " << oldShininess << " -> " << currentMaterialData.shininess 
                  << " (delta: " << delta << ")" << std::endl;
    }
    
    int currentMaterialPreset = 0;
    void cycleMaterialPreset() {
        currentMaterialPreset = (currentMaterialPreset + 1) % 5;
        
        switch (currentMaterialPreset) {
            case 0: // Default material
                currentMaterialData.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 0.0f);
                currentMaterialData.diffuse = glm::vec4(0.8f, 0.6f, 0.4f, 0.0f);
                currentMaterialData.specular = glm::vec4(0.3f, 0.3f, 0.3f, 0.0f);
                currentMaterialData.shininess = 32.0f;
                std::cout << "[MATERIAL] Preset: Default (Warm Brown)" << std::endl;
                break;
            case 1: // Metallic gold
                currentMaterialData.ambient = glm::vec4(0.24725f, 0.1995f, 0.0745f, 0.0f);
                currentMaterialData.diffuse = glm::vec4(0.75164f, 0.60648f, 0.22648f, 0.0f);
                currentMaterialData.specular = glm::vec4(0.628281f, 0.555802f, 0.366065f, 0.0f);
                currentMaterialData.shininess = 51.2f;
                std::cout << "[MATERIAL] Preset: Metallic Gold" << std::endl;
                break;
            case 2: // Ruby red
                currentMaterialData.ambient = glm::vec4(0.1745f, 0.01175f, 0.01175f, 0.0f);
                currentMaterialData.diffuse = glm::vec4(0.61424f, 0.04136f, 0.04136f, 0.0f);
                currentMaterialData.specular = glm::vec4(0.727811f, 0.626959f, 0.626959f, 0.0f);
                currentMaterialData.shininess = 76.8f;
                std::cout << "[MATERIAL] Preset: Ruby Red" << std::endl;
                break;
            case 3: // Chrome
                currentMaterialData.ambient = glm::vec4(0.25f, 0.25f, 0.25f, 0.0f);
                currentMaterialData.diffuse = glm::vec4(0.4f, 0.4f, 0.4f, 0.0f);
                currentMaterialData.specular = glm::vec4(0.774597f, 0.774597f, 0.774597f, 0.0f);
                currentMaterialData.shininess = 76.8f;
                std::cout << "[MATERIAL] Preset: Chrome" << std::endl;
                break;
            case 4: // Emerald green
                currentMaterialData.ambient = glm::vec4(0.0215f, 0.1745f, 0.0215f, 0.0f);
                currentMaterialData.diffuse = glm::vec4(0.07568f, 0.61424f, 0.07568f, 0.0f);
                currentMaterialData.specular = glm::vec4(0.633f, 0.727811f, 0.633f, 0.0f);
                currentMaterialData.shininess = 76.8f;
                std::cout << "[MATERIAL] Preset: Emerald Green" << std::endl;
                break;
        }
        updateMaterialBuffer(); // Update the simple material buffer
    }

    void cleanup() {
        VKMON_INFO("Beginning VulkanMon cleanup...");
        
        // Cleanup core engine systems
        if (currentModel) {
            VKMON_INFO("Cleaning up loaded model...");
            currentModel.reset();
        }
        
        if (modelLoader) {
            modelLoader->printLoadingSummary();
            modelLoader.reset();
        }
        
        if (assetManager) {
            assetManager->printAssetSummary();
            assetManager.reset();
        }
        
        if (lightingSystem) {
            lightingSystem.reset();
        }
        
        if (materialSystem) {
            materialSystem.reset();
        }
        
        if (resourceManager) {
            resourceManager->printResourceSummary();
            resourceManager.reset();
        }
        
        // Cleanup depth buffer resources (with null checks)
        if (depthImageView != VK_NULL_HANDLE) vkDestroyImageView(device, depthImageView, nullptr);
        if (depthImage != VK_NULL_HANDLE) vkDestroyImage(device, depthImage, nullptr);
        if (depthImageMemory != VK_NULL_HANDLE) vkFreeMemory(device, depthImageMemory, nullptr);
        
        // Cleanup UBO resources (with null checks)
        if (uniformBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, uniformBuffer, nullptr);
        if (uniformBufferMemory != VK_NULL_HANDLE) vkFreeMemory(device, uniformBufferMemory, nullptr);
        
        // Cleanup texture resources (with null checks)
        if (textureSampler != VK_NULL_HANDLE) vkDestroySampler(device, textureSampler, nullptr);
        if (textureImageView != VK_NULL_HANDLE) vkDestroyImageView(device, textureImageView, nullptr);
        if (textureImage != VK_NULL_HANDLE) vkDestroyImage(device, textureImage, nullptr);
        if (textureImageMemory != VK_NULL_HANDLE) vkFreeMemory(device, textureImageMemory, nullptr);
        if (descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        if (descriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        
        // Cleanup remaining Vulkan resources
        if (materialBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, materialBuffer, nullptr);
        }
        if (materialBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, materialBufferMemory, nullptr);
        }
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    HelloTriangleApp app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}