#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace VulkanMon {
    class ResourceManager;
    class ManagedBuffer;
    struct Material; // Forward declaration from ModelLoader.h
}

struct MaterialData {
    glm::vec4 ambient;   // Using vec4 for proper alignment
    glm::vec4 diffuse;   // Using vec4 for proper alignment  
    glm::vec4 specular;  // Using vec4 for proper alignment
    float shininess;
    glm::vec3 _padding;  // Align to 16-byte boundary
    
    MaterialData() 
        : ambient(0.1f, 0.1f, 0.1f, 0.0f), diffuse(0.8f, 0.8f, 0.8f, 0.0f), 
          specular(1.0f, 1.0f, 1.0f, 0.0f), shininess(32.0f), _padding(0.0f) {}
    
    MaterialData(const glm::vec3& amb, const glm::vec3& diff, const glm::vec3& spec, float shin)
        : ambient(amb.x, amb.y, amb.z, 0.0f), diffuse(diff.x, diff.y, diff.z, 0.0f), 
          specular(spec.x, spec.y, spec.z, 0.0f), shininess(shin), _padding(0.0f) {}
};

class MaterialSystem {
public:
    MaterialSystem(std::shared_ptr<VulkanMon::ResourceManager> resourceManager);
    ~MaterialSystem();
    
    void createMaterialBuffers();
    void updateMaterial(uint32_t materialId, const MaterialData& materialData);
    void updateMaterialFromModelMaterial(uint32_t materialId, const VulkanMon::Material& material);
    
    uint32_t createMaterial(const MaterialData& materialData);
    uint32_t createMaterialFromModelMaterial(const VulkanMon::Material& material);
    
    VkBuffer getMaterialBuffer(uint32_t materialId) const;
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }
    VkDescriptorSet getDescriptorSet(uint32_t materialId) const;
    
    const MaterialData& getMaterialData(uint32_t materialId) const;
    size_t getMaterialCount() const { return materials.size(); }
    
    void cleanup();

private:
    std::shared_ptr<VulkanMon::ResourceManager> resourceManager;
    
    struct MaterialBufferSet {
        std::unique_ptr<VulkanMon::ManagedBuffer> materialBufferManaged;
        VkBuffer materialBuffer;
        VkDeviceMemory materialBufferMemory;
        VkDescriptorSet descriptorSet;
        MaterialData data;
    };
    
    std::vector<MaterialBufferSet> materials;
    std::unordered_map<std::string, uint32_t> materialNameMap;
    
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSet(uint32_t materialId);
    void updateDescriptorSet(uint32_t materialId);
    void ensureDescriptorPoolCapacity(size_t requiredCapacity);
    
    uint32_t nextMaterialId = 0;
};