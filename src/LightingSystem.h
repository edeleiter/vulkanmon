#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <memory>

namespace VulkanMon {
    class ResourceManager;
    class ManagedBuffer;
}

struct DirectionalLight {
    glm::vec3 direction;
    float intensity;
    glm::vec3 color;
    float _padding;
    
    DirectionalLight(const glm::vec3& dir = glm::vec3(0.0f, -1.0f, 0.0f), 
                     float intens = 1.0f, 
                     const glm::vec3& col = glm::vec3(1.0f, 1.0f, 1.0f))
        : direction(glm::normalize(dir)), intensity(intens), color(col), _padding(0.0f) {}
};

struct LightingData {
    DirectionalLight directionalLight;
    glm::vec3 ambientColor;
    float ambientIntensity;
    
    LightingData() 
        : directionalLight(), ambientColor(0.2f, 0.2f, 0.3f), ambientIntensity(0.3f) {}
};

class LightingSystem {
public:
    LightingSystem(std::shared_ptr<VulkanMon::ResourceManager> resourceManager);
    ~LightingSystem();
    
    void createLightingBuffers();
    void updateLighting(const LightingData& lightingData);
    
    VkBuffer getLightingBuffer() const { return lightingBuffer; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }
    VkDescriptorSet getDescriptorSet() const { return descriptorSet; }
    
    const LightingData& getCurrentLighting() const { return currentLighting; }
    LightingData& getCurrentLighting() { return currentLighting; }
    
    void setDirectionalLight(const glm::vec3& direction, float intensity, const glm::vec3& color);
    void setAmbientLight(const glm::vec3& color, float intensity);
    
    void cleanup();

private:
    std::shared_ptr<VulkanMon::ResourceManager> resourceManager;
    
    std::unique_ptr<VulkanMon::ManagedBuffer> lightingBufferManaged;
    VkBuffer lightingBuffer;
    VkDeviceMemory lightingBufferMemory;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    
    LightingData currentLighting;
    
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSet();
    void updateDescriptorSet();
};