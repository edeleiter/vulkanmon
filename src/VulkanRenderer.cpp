#include "VulkanRenderer.h"
#include "Utils.h"
#include "ModelLoader.h"
#include <iostream>
#include <stdexcept>
#include <array>

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

void VulkanRenderer::handleWindowResize() {
    // TODO: Implement swapchain recreation
    VKMON_INFO("Window resize handling not yet implemented");
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

// =============================================================================
// Vulkan Initialization Methods (extracted from main.cpp)
// =============================================================================

void VulkanRenderer::initVulkan() {
    VKMON_INFO("Starting Vulkan initialization sequence...");
    
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
    
    VKMON_INFO("Vulkan initialization completed");
}

void VulkanRenderer::createInstance() {
    VKMON_INFO("Creating Vulkan instance...");
    
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

    VKMON_INFO("Vulkan instance created successfully");
}

void VulkanRenderer::createSurface() {
    VKMON_INFO("Creating Vulkan surface...");
    
    window_->createSurface(instance_);
    surface_ = window_->getSurface();
    
    VKMON_INFO("Window surface created successfully");
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
    VKMON_INFO("Logical device created successfully");
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
    VKMON_INFO("Creating swapchain...");
    
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

    VKMON_INFO("Render pass created successfully");
}

void VulkanRenderer::createShaderModules() {
    VKMON_INFO("Creating shader modules...");
    
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

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

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

    VKMON_INFO("Graphics pipeline created successfully");
}

void VulkanRenderer::createFramebuffers() {
    // TODO: Extract implementation from main.cpp
    VKMON_INFO("Creating framebuffers (placeholder)");
    throw std::runtime_error("VulkanRenderer::createFramebuffers() not yet implemented - needs extraction from main.cpp");
}

void VulkanRenderer::createCommandPool() {
    // TODO: Extract implementation from main.cpp
    VKMON_INFO("Creating command pool (placeholder)");
    throw std::runtime_error("VulkanRenderer::createCommandPool() not yet implemented - needs extraction from main.cpp");
}

void VulkanRenderer::createCommandBuffers() {
    // TODO: Extract implementation from main.cpp
    VKMON_INFO("Creating command buffers (placeholder)");
    throw std::runtime_error("VulkanRenderer::createCommandBuffers() not yet implemented - needs extraction from main.cpp");
}

void VulkanRenderer::createSyncObjects() {
    // TODO: Extract implementation from main.cpp
    VKMON_INFO("Creating synchronization objects (placeholder)");
    throw std::runtime_error("VulkanRenderer::createSyncObjects() not yet implemented - needs extraction from main.cpp");
}

// =============================================================================
// Resource Creation Methods (to be extracted from main.cpp)
// =============================================================================

void VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                                VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    // TODO: Extract implementation from main.cpp
    throw std::runtime_error("VulkanRenderer::createBuffer() not yet implemented");
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
    VKMON_INFO("Creating descriptor set layout...");
    
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
        throw std::runtime_error("Failed to create descriptor set layout!");
    }

    VKMON_INFO("Descriptor set layout created successfully");
}

void VulkanRenderer::createTextureImage() {
    // TODO: Extract implementation from main.cpp
    throw std::runtime_error("VulkanRenderer::createTextureImage() not yet implemented");
}

void VulkanRenderer::createTextureImageView() {
    // TODO: Extract implementation from main.cpp
    throw std::runtime_error("VulkanRenderer::createTextureImageView() not yet implemented");
}

void VulkanRenderer::createTextureSampler() {
    // TODO: Extract implementation from main.cpp
    throw std::runtime_error("VulkanRenderer::createTextureSampler() not yet implemented");
}

void VulkanRenderer::createDescriptorPool() {
    // TODO: Extract implementation from main.cpp
    throw std::runtime_error("VulkanRenderer::createDescriptorPool() not yet implemented");
}

void VulkanRenderer::createDescriptorSet() {
    // TODO: Extract implementation from main.cpp
    throw std::runtime_error("VulkanRenderer::createDescriptorSet() not yet implemented");
}

void VulkanRenderer::createUniformBuffer() {
    // TODO: Extract implementation from main.cpp
    throw std::runtime_error("VulkanRenderer::createUniformBuffer() not yet implemented");
}

void VulkanRenderer::createMaterialBuffer() {
    // TODO: Extract implementation from main.cpp
    throw std::runtime_error("VulkanRenderer::createMaterialBuffer() not yet implemented");
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
    // TODO: Extract implementation from main.cpp
    // For now, just do nothing so we don't crash
    // throw std::runtime_error("VulkanRenderer::drawFrame() not yet implemented");
}

void VulkanRenderer::recordCommandBuffers() {
    // TODO: Extract implementation from main.cpp
    throw std::runtime_error("VulkanRenderer::recordCommandBuffers() not yet implemented");
}

void VulkanRenderer::updateUniformBuffer() {
    // TODO: Extract implementation from main.cpp
    throw std::runtime_error("VulkanRenderer::updateUniformBuffer() not yet implemented");
}

void VulkanRenderer::updateMaterialBuffer() {
    // TODO: Extract implementation from main.cpp
    throw std::runtime_error("VulkanRenderer::updateMaterialBuffer() not yet implemented");
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
    VKMON_INFO("Initializing core systems (temporary implementation)...");
    
    // This is a temporary solution until we create VulkanContext
    // These systems need Vulkan device which we don't have until after createLogicalDevice()
    
    VKMON_INFO("Core systems initialization postponed until VulkanContext is created");
    
    // TODO: Create ResourceManager, AssetManager, etc. here
    // For now, we'll leave them as nullptr and handle this in the next step
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