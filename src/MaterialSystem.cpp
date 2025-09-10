#include "MaterialSystem.h"
#include "ResourceManager.h"
#include "ModelLoader.h"
#include "Logger.h"
#include <stdexcept>

MaterialSystem::MaterialSystem(std::shared_ptr<VulkanMon::ResourceManager> resourceManager)
    : resourceManager(resourceManager), descriptorSetLayout(VK_NULL_HANDLE), descriptorPool(VK_NULL_HANDLE) {
    VKMON_INFO("MaterialSystem: Initializing material management system");
}

MaterialSystem::~MaterialSystem() {
    cleanup();
}

void MaterialSystem::createMaterialBuffers() {
    VKMON_INFO("MaterialSystem: Creating material descriptor layout and pool");
    createDescriptorSetLayout();
    createDescriptorPool();
}

void MaterialSystem::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding materialBinding{};
    materialBinding.binding = 3; // Binding 3 for material data (after MVP=0, texture=1, lighting=2)
    materialBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    materialBinding.descriptorCount = 1;
    materialBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    materialBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &materialBinding;

    VkDevice device = resourceManager->getDevice();
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("MaterialSystem: Failed to create descriptor set layout");
    }
    
    VKMON_INFO("MaterialSystem: Created material descriptor set layout");
}

void MaterialSystem::createDescriptorPool() {
    // Start with capacity for 16 materials, will expand as needed
    ensureDescriptorPoolCapacity(16);
}

void MaterialSystem::ensureDescriptorPoolCapacity(size_t requiredCapacity) {
    if (descriptorPool != VK_NULL_HANDLE && materials.capacity() >= requiredCapacity) {
        return; // Current pool is sufficient
    }
    
    // Clean up existing pool if it exists
    if (descriptorPool != VK_NULL_HANDLE) {
        VkDevice device = resourceManager->getDevice();
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        VKMON_INFO("MaterialSystem: Destroyed old descriptor pool for expansion");
    }
    
    // Create new pool with increased capacity
    size_t poolSize = std::max(requiredCapacity, size_t(32));
    
    VkDescriptorPoolSize poolSizes{};
    poolSizes.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes.descriptorCount = static_cast<uint32_t>(poolSize);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSizes;
    poolInfo.maxSets = static_cast<uint32_t>(poolSize);
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VkDevice device = resourceManager->getDevice();
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("MaterialSystem: Failed to create descriptor pool");
    }
    
    VKMON_INFO("MaterialSystem: Created descriptor pool with capacity for " + std::to_string(poolSize) + " materials");
    
    // Reserve vector capacity to match descriptor pool
    materials.reserve(poolSize);
    
    // Recreate descriptor sets for existing materials
    for (size_t i = 0; i < materials.size(); ++i) {
        createDescriptorSet(static_cast<uint32_t>(i));
    }
}

uint32_t MaterialSystem::createMaterial(const MaterialData& materialData) {
    ensureDescriptorPoolCapacity(materials.size() + 1);
    
    uint32_t materialId = nextMaterialId++;
    
    MaterialBufferSet bufferSet;
    bufferSet.data = materialData;
    
    // Create uniform buffer
    VkDeviceSize bufferSize = sizeof(MaterialData);
    bufferSet.materialBufferManaged = resourceManager->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    
    bufferSet.materialBuffer = bufferSet.materialBufferManaged->getBuffer();
    bufferSet.materialBufferMemory = bufferSet.materialBufferManaged->getMemory();
    
    // Add to materials vector
    if (materialId >= materials.size()) {
        materials.resize(materialId + 1);
    }
    materials[materialId] = std::move(bufferSet);
    
    // Create descriptor set
    createDescriptorSet(materialId);
    
    // Update the buffer with material data
    updateMaterial(materialId, materialData);
    
    VKMON_INFO("MaterialSystem: Created material " + std::to_string(materialId));
    return materialId;
}

uint32_t MaterialSystem::createMaterialFromModelMaterial(const VulkanMon::Material& material) {
    MaterialData materialData(material.ambient, material.diffuse, material.specular, material.shininess);
    
    uint32_t materialId = createMaterial(materialData);
    
    // Store name mapping if the material has a name
    if (!material.name.empty()) {
        materialNameMap[material.name] = materialId;
        VKMON_INFO("MaterialSystem: Mapped material name '" + material.name + "' to ID " + std::to_string(materialId));
    }
    
    return materialId;
}

void MaterialSystem::createDescriptorSet(uint32_t materialId) {
    if (materialId >= materials.size()) {
        throw std::runtime_error("MaterialSystem: Invalid material ID for descriptor set creation");
    }
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    VkDevice device = resourceManager->getDevice();
    if (vkAllocateDescriptorSets(device, &allocInfo, &materials[materialId].descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("MaterialSystem: Failed to allocate descriptor set for material " + std::to_string(materialId));
    }
    
    updateDescriptorSet(materialId);
}

void MaterialSystem::updateDescriptorSet(uint32_t materialId) {
    if (materialId >= materials.size()) {
        throw std::runtime_error("MaterialSystem: Invalid material ID for descriptor set update");
    }
    
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = materials[materialId].materialBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(MaterialData);

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = materials[materialId].descriptorSet;
    descriptorWrite.dstBinding = 3;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    VkDevice device = resourceManager->getDevice();
    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
}

void MaterialSystem::updateMaterial(uint32_t materialId, const MaterialData& materialData) {
    if (materialId >= materials.size()) {
        throw std::runtime_error("MaterialSystem: Invalid material ID for update");
    }
    
    materials[materialId].data = materialData;
    
    // Update the uniform buffer
    void* data;
    VkDevice device = resourceManager->getDevice();
    vkMapMemory(device, materials[materialId].materialBufferMemory, 0, sizeof(MaterialData), 0, &data);
    memcpy(data, &materialData, sizeof(MaterialData));
    vkUnmapMemory(device, materials[materialId].materialBufferMemory);
}

void MaterialSystem::updateMaterialFromModelMaterial(uint32_t materialId, const VulkanMon::Material& material) {
    MaterialData materialData(material.ambient, material.diffuse, material.specular, material.shininess);
    updateMaterial(materialId, materialData);
}

VkBuffer MaterialSystem::getMaterialBuffer(uint32_t materialId) const {
    if (materialId >= materials.size()) {
        throw std::runtime_error("MaterialSystem: Invalid material ID for buffer access");
    }
    return materials[materialId].materialBuffer;
}

VkDescriptorSet MaterialSystem::getDescriptorSet(uint32_t materialId) const {
    if (materialId >= materials.size()) {
        throw std::runtime_error("MaterialSystem: Invalid material ID for descriptor set access");
    }
    return materials[materialId].descriptorSet;
}

const MaterialData& MaterialSystem::getMaterialData(uint32_t materialId) const {
    if (materialId >= materials.size()) {
        throw std::runtime_error("MaterialSystem: Invalid material ID for data access");
    }
    return materials[materialId].data;
}

void MaterialSystem::cleanup() {
    VkDevice device = resourceManager->getDevice();
    
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
        VKMON_INFO("MaterialSystem: Destroyed descriptor pool");
    }
    
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
        VKMON_INFO("MaterialSystem: Destroyed descriptor set layout");
    }
    
    materials.clear();
    materialNameMap.clear();
    
    VKMON_INFO("MaterialSystem: Cleanup completed");
}