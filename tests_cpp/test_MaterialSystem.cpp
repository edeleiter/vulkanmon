/**
 * VulkanMon MaterialSystem Unit Tests
 * 
 * Testing material properties, material data structures, and Vulkan integration
 * 
 * Test Categories:
 * - MaterialData structure validation
 * - Material system interface design
 * - Material creation and management
 * - Property validation and alignment
 * - Error handling
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/systems/MaterialSystem.h"
#include "../src/rendering/ResourceManager.h"
#include "fixtures/TestHelpers.h"
#include <memory>
#include <type_traits>
#include <chrono>
#include <vector>

using namespace VulkanMon::Testing;
using namespace VulkanMon;  // Add this to access MaterialData
using Catch::Approx;

TEST_CASE("MaterialData Structure Validation", "[MaterialSystem][MaterialData]") {
    SECTION("Default constructor properties") {
        MaterialData material;
        
        // Test default ambient values
        REQUIRE(material.ambient.r == Approx(0.1f));
        REQUIRE(material.ambient.g == Approx(0.1f));
        REQUIRE(material.ambient.b == Approx(0.1f));
        REQUIRE(material.ambient.a == Approx(0.0f));
        
        // Test default diffuse values
        REQUIRE(material.diffuse.r == Approx(0.8f));
        REQUIRE(material.diffuse.g == Approx(0.8f));
        REQUIRE(material.diffuse.b == Approx(0.8f));
        REQUIRE(material.diffuse.a == Approx(0.0f));
        
        // Test default specular values
        REQUIRE(material.specular.r == Approx(1.0f));
        REQUIRE(material.specular.g == Approx(1.0f));
        REQUIRE(material.specular.b == Approx(1.0f));
        REQUIRE(material.specular.a == Approx(0.0f));
        
        // Test default shininess
        REQUIRE(material.shininess == Approx(32.0f));
    }
    
    SECTION("Custom constructor properties") {
        glm::vec3 ambient(0.2f, 0.3f, 0.4f);
        glm::vec3 diffuse(0.5f, 0.6f, 0.7f);
        glm::vec3 specular(0.8f, 0.9f, 1.0f);
        float shininess = 64.0f;
        
        MaterialData material(ambient, diffuse, specular, shininess);
        
        // Test custom ambient values
        REQUIRE(material.ambient.r == Approx(0.2f));
        REQUIRE(material.ambient.g == Approx(0.3f));
        REQUIRE(material.ambient.b == Approx(0.4f));
        REQUIRE(material.ambient.a == Approx(0.0f));
        
        // Test custom diffuse values
        REQUIRE(material.diffuse.r == Approx(0.5f));
        REQUIRE(material.diffuse.g == Approx(0.6f));
        REQUIRE(material.diffuse.b == Approx(0.7f));
        REQUIRE(material.diffuse.a == Approx(0.0f));
        
        // Test custom specular values
        REQUIRE(material.specular.r == Approx(0.8f));
        REQUIRE(material.specular.g == Approx(0.9f));
        REQUIRE(material.specular.b == Approx(1.0f));
        REQUIRE(material.specular.a == Approx(0.0f));
        
        // Test custom shininess
        REQUIRE(material.shininess == Approx(64.0f));
    }
    
    SECTION("Memory layout and alignment") {
        // Test that MaterialData has proper size for GPU alignment
        REQUIRE(sizeof(MaterialData) >= sizeof(glm::vec4) * 3 + sizeof(float));
        
        // Test that vectors are properly aligned
        MaterialData material;
        
        // Verify vec4 alignment (should be 16-byte aligned)
        REQUIRE(sizeof(material.ambient) == 16);
        REQUIRE(sizeof(material.diffuse) == 16);
        REQUIRE(sizeof(material.specular) == 16);
        
        // Test that shininess is a proper float
        REQUIRE(sizeof(material.shininess) == sizeof(float));
    }
}

// REMOVED: Interface Design test - silly validation of method existence

// REMOVED: Method Interface Validation test - silly validation of method existence

TEST_CASE("MaterialSystem Property Ranges", "[MaterialSystem][Properties]") {
    SECTION("Valid material property ranges") {
        // Test various material property combinations
        std::vector<MaterialData> testMaterials = {
            // Default material
            MaterialData(),
            
            // Gold-like material
            MaterialData(
                glm::vec3(0.24725f, 0.1995f, 0.0745f),   // ambient
                glm::vec3(0.75164f, 0.60648f, 0.22648f), // diffuse
                glm::vec3(0.628281f, 0.555802f, 0.366065f), // specular
                51.2f
            ),
            
            // Chrome-like material  
            MaterialData(
                glm::vec3(0.25f, 0.25f, 0.25f),         // ambient
                glm::vec3(0.4f, 0.4f, 0.4f),           // diffuse
                glm::vec3(0.774597f, 0.774597f, 0.774597f), // specular
                76.8f
            ),
            
            // Ruby-like material
            MaterialData(
                glm::vec3(0.1745f, 0.01175f, 0.01175f), // ambient
                glm::vec3(0.61424f, 0.04136f, 0.04136f), // diffuse
                glm::vec3(0.727811f, 0.626959f, 0.626959f), // specular
                76.8f
            )
        };
        
        for (const auto& material : testMaterials) {
            // Test ambient range [0,1]
            REQUIRE(material.ambient.r >= 0.0f);
            REQUIRE(material.ambient.g >= 0.0f);
            REQUIRE(material.ambient.b >= 0.0f);
            REQUIRE(material.ambient.r <= 1.0f);
            REQUIRE(material.ambient.g <= 1.0f);
            REQUIRE(material.ambient.b <= 1.0f);
            
            // Test diffuse range [0,1]
            REQUIRE(material.diffuse.r >= 0.0f);
            REQUIRE(material.diffuse.g >= 0.0f);
            REQUIRE(material.diffuse.b >= 0.0f);
            REQUIRE(material.diffuse.r <= 1.0f);
            REQUIRE(material.diffuse.g <= 1.0f);
            REQUIRE(material.diffuse.b <= 1.0f);
            
            // Test specular range [0,1]
            REQUIRE(material.specular.r >= 0.0f);
            REQUIRE(material.specular.g >= 0.0f);
            REQUIRE(material.specular.b >= 0.0f);
            REQUIRE(material.specular.r <= 1.0f);
            REQUIRE(material.specular.g <= 1.0f);
            REQUIRE(material.specular.b <= 1.0f);
            
            // Test shininess is positive
            REQUIRE(material.shininess > 0.0f);
            REQUIRE(material.shininess <= 1000.0f); // Reasonable upper bound
        }
    }
    
    SECTION("Edge case material properties") {
        // Test edge cases for material properties
        
        // Minimum values
        MaterialData minMaterial(
            glm::vec3(0.0f, 0.0f, 0.0f),  // black ambient
            glm::vec3(0.0f, 0.0f, 0.0f),  // black diffuse
            glm::vec3(0.0f, 0.0f, 0.0f),  // black specular
            0.1f                           // very low shininess
        );
        
        REQUIRE(minMaterial.ambient.r == Approx(0.0f));
        REQUIRE(minMaterial.diffuse.r == Approx(0.0f));
        REQUIRE(minMaterial.specular.r == Approx(0.0f));
        REQUIRE(minMaterial.shininess == Approx(0.1f));
        
        // Maximum values
        MaterialData maxMaterial(
            glm::vec3(1.0f, 1.0f, 1.0f),  // white ambient
            glm::vec3(1.0f, 1.0f, 1.0f),  // white diffuse
            glm::vec3(1.0f, 1.0f, 1.0f),  // white specular
            1000.0f                        // very high shininess
        );
        
        REQUIRE(maxMaterial.ambient.r == Approx(1.0f));
        REQUIRE(maxMaterial.diffuse.r == Approx(1.0f));
        REQUIRE(maxMaterial.specular.r == Approx(1.0f));
        REQUIRE(maxMaterial.shininess == Approx(1000.0f));
    }
}

TEST_CASE("MaterialSystem Vulkan Integration", "[MaterialSystem][Vulkan]") {
    SECTION("Vulkan type validation") {
        // Test that Vulkan types are used correctly
        
        // Buffer types
        VkBuffer testBuffer = VK_NULL_HANDLE;
        VkDeviceMemory testMemory = VK_NULL_HANDLE;
        REQUIRE(testBuffer == VK_NULL_HANDLE);
        REQUIRE(testMemory == VK_NULL_HANDLE);
        
        // Descriptor types
        VkDescriptorSetLayout testLayout = VK_NULL_HANDLE;
        VkDescriptorSet testDescriptorSet = VK_NULL_HANDLE;
        VkDescriptorPool testPool = VK_NULL_HANDLE;
        REQUIRE(testLayout == VK_NULL_HANDLE);
        REQUIRE(testDescriptorSet == VK_NULL_HANDLE);
        REQUIRE(testPool == VK_NULL_HANDLE);
        
        // Device size validation
        VkDeviceSize testSize = sizeof(MaterialData);
        REQUIRE(testSize > 0);
        REQUIRE(testSize >= 64); // MaterialData should be at least 64 bytes
    }
    
    SECTION("Material buffer size calculations") {
        // Test that buffer size calculations are correct
        VkDeviceSize materialDataSize = sizeof(MaterialData);
        
        // MaterialData should have proper GPU alignment
        REQUIRE(materialDataSize >= 64); // Minimum expected size
        REQUIRE(materialDataSize % 16 == 0); // Should be 16-byte aligned
        
        // Verify individual component sizes
        REQUIRE(sizeof(glm::vec4) == 16);
        REQUIRE(sizeof(float) == 4);
        
        // Total size should account for padding
        size_t expectedMinSize = sizeof(glm::vec4) * 3 + sizeof(float); // 52 bytes minimum
        REQUIRE(materialDataSize >= expectedMinSize);
    }
}

TEST_CASE("MaterialSystem Performance Characteristics", "[MaterialSystem][Performance]") {
    SECTION("Material data creation performance") {
        // Test that material creation is efficient
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Create many materials to test performance
        std::vector<MaterialData> materials;
        materials.reserve(1000);
        
        for (int i = 0; i < 1000; ++i) {
            float factor = static_cast<float>(i) / 1000.0f;
            materials.emplace_back(
                glm::vec3(factor * 0.2f, factor * 0.2f, factor * 0.2f),
                glm::vec3(factor * 0.8f, factor * 0.8f, factor * 0.8f),
                glm::vec3(1.0f, 1.0f, 1.0f),
                32.0f + factor * 100.0f
            );
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // Material creation should be very fast (under 10ms for 1000 materials)
        REQUIRE(duration.count() < 10);
        REQUIRE(materials.size() == 1000);
        
        // Verify first and last materials
        REQUIRE(materials[0].ambient.r == Approx(0.0f));
        REQUIRE(materials[999].ambient.r == Approx(0.1998f).margin(0.001f));
    }
    
    SECTION("MaterialData memory efficiency") {
        // Test memory usage characteristics
        MaterialData material;
        
        // Material should not be excessively large
        REQUIRE(sizeof(MaterialData) <= 128); // Reasonable upper bound
        
        // But should be large enough for GPU alignment
        REQUIRE(sizeof(MaterialData) >= 64);
        
        // Test that we can create many materials efficiently
        std::vector<MaterialData> materials(10000);
        size_t totalMemory = materials.size() * sizeof(MaterialData);
        
        // 10000 materials should use reasonable memory (less than 2MB)
        REQUIRE(totalMemory < 2 * 1024 * 1024);
    }
}