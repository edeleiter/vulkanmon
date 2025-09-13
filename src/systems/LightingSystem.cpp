#include "LightingSystem.h"
#include "../rendering/ResourceManager.h"
#include "../utils/Logger.h"
#include <cstring>

using namespace VulkanMon;

LightingSystem::LightingSystem(std::shared_ptr<VulkanMon::ResourceManager> resourceManager)
    : resourceManager(resourceManager)
    , lightingBuffer(VK_NULL_HANDLE)
    , lightingBufferMemory(VK_NULL_HANDLE) 
    , descriptorSetLayout(VK_NULL_HANDLE)
    , descriptorPool(VK_NULL_HANDLE)
    , descriptorSet(VK_NULL_HANDLE) {
    
    VKMON_INFO("Initializing LightingSystem");
}

LightingSystem::~LightingSystem() {
    cleanup();
}

void LightingSystem::createLightingBuffers() {
    VKMON_INFO("Creating lighting uniform buffers");
    
    VkDeviceSize bufferSize = sizeof(LightingData);
    
    lightingBufferManaged = resourceManager->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "LightingUniformBuffer"
    );
    
    if (!lightingBufferManaged) {
        throw std::runtime_error("Failed to create lighting uniform buffer");
    }
    
    lightingBuffer = lightingBufferManaged->getBuffer();
    lightingBufferMemory = lightingBufferManaged->getMemory();
    
    createDescriptorSetLayout();
    createDescriptorPool();
    createDescriptorSet();
    
    // Initialize with default lighting
    updateLighting(currentLighting);
    
    VKMON_INFO("LightingSystem buffers created successfully");
}

void LightingSystem::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding lightingLayoutBinding{};
    lightingLayoutBinding.binding = 0;
    lightingLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightingLayoutBinding.descriptorCount = 1;
    lightingLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    lightingLayoutBinding.pImmutableSamplers = nullptr;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &lightingLayoutBinding;
    
    VkResult result = vkCreateDescriptorSetLayout(
        resourceManager->getDevice(),
        &layoutInfo,
        nullptr,
        &descriptorSetLayout
    );
    
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create lighting descriptor set layout");
    }
}

void LightingSystem::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;
    
    VkResult result = vkCreateDescriptorPool(
        resourceManager->getDevice(),
        &poolInfo,
        nullptr,
        &descriptorPool
    );
    
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create lighting descriptor pool");
    }
}

void LightingSystem::createDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    
    VkResult result = vkAllocateDescriptorSets(
        resourceManager->getDevice(),
        &allocInfo,
        &descriptorSet
    );
    
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate lighting descriptor set");
    }
    
    updateDescriptorSet();
}

void LightingSystem::updateDescriptorSet() {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = lightingBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(LightingData);
    
    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;
    
    vkUpdateDescriptorSets(
        resourceManager->getDevice(),
        1,
        &descriptorWrite,
        0,
        nullptr
    );
}

void LightingSystem::updateLighting(const LightingData& lightingData) {
    currentLighting = lightingData;
    
    void* data;
    VkResult result = vkMapMemory(
        resourceManager->getDevice(),
        lightingBufferMemory,
        0,
        sizeof(LightingData),
        0,
        &data
    );
    
    if (result != VK_SUCCESS) {
        VKMON_ERROR("Failed to map lighting buffer memory");
        return;
    }
    
    std::memcpy(data, &currentLighting, sizeof(LightingData));
    vkUnmapMemory(resourceManager->getDevice(), lightingBufferMemory);
}

void LightingSystem::setDirectionalLight(const glm::vec3& direction, float intensity, const glm::vec3& color) {
    currentLighting.directionalLight.direction = glm::normalize(direction);
    currentLighting.directionalLight.intensity = intensity;
    currentLighting.directionalLight.color = color;
    updateLighting(currentLighting);
    
    VKMON_DEBUG("Updated directional light");
}

void LightingSystem::setAmbientLight(const glm::vec3& color, float intensity) {
    currentLighting.ambientColor = color;
    currentLighting.ambientIntensity = intensity;
    updateLighting(currentLighting);
    
    VKMON_DEBUG("Updated ambient light");
}

void LightingSystem::cleanup() {
    if (resourceManager && resourceManager->getDevice() != VK_NULL_HANDLE) {
        if (descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(resourceManager->getDevice(), descriptorSetLayout, nullptr);
            descriptorSetLayout = VK_NULL_HANDLE;
        }
        
        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(resourceManager->getDevice(), descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }
        
        lightingBufferManaged.reset();
        lightingBuffer = VK_NULL_HANDLE;
        lightingBufferMemory = VK_NULL_HANDLE;
    }
    
    VKMON_INFO("LightingSystem cleanup completed");
}