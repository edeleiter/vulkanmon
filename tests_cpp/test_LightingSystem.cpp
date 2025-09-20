/**
 * VulkanMon LightingSystem Unit Tests
 * 
 * Testing lighting calculations, directional lights, ambient lighting
 * 
 * Test Categories:
 * - Lighting system initialization
 * - Directional light controls
 * - Ambient lighting
 * - Lighting presets
 * - Descriptor management
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/systems/LightingSystem.h"
#include "fixtures/TestHelpers.h"
#include <memory>

using namespace VulkanMon::Testing;
using Catch::Approx;

TEST_CASE("DirectionalLight Structure", "[LightingSystem][DirectionalLight]") {
    SECTION("Default construction") {
        DirectionalLight light;
        REQUIRE(light.direction.x == Approx(0.0f));
        REQUIRE(light.direction.y == Approx(-1.0f));
        REQUIRE(light.direction.z == Approx(0.0f));
        REQUIRE(light.intensity == Approx(1.0f));
        REQUIRE(light.color.r == Approx(1.0f));
        REQUIRE(light.color.g == Approx(1.0f));
        REQUIRE(light.color.b == Approx(1.0f));
    }
    
    SECTION("Custom construction") {
        glm::vec3 dir(1.0f, -1.0f, 0.0f);
        float intensity = 0.8f;
        glm::vec3 color(1.0f, 0.8f, 0.6f);
        
        DirectionalLight light(dir, intensity, color);
        
        // Direction should be normalized
        glm::vec3 expectedDir = glm::normalize(dir);
        REQUIRE(light.direction.x == Approx(expectedDir.x));
        REQUIRE(light.direction.y == Approx(expectedDir.y));
        REQUIRE(light.direction.z == Approx(expectedDir.z));
        
        REQUIRE(light.intensity == Approx(intensity));
        REQUIRE(light.color.r == Approx(color.r));
        REQUIRE(light.color.g == Approx(color.g));
        REQUIRE(light.color.b == Approx(color.b));
    }
}

TEST_CASE("LightingData Structure", "[LightingSystem][LightingData]") {
    SECTION("Default construction") {
        LightingData data;
        
        // Should have valid directional light
        REQUIRE(data.directionalLight.intensity == Approx(1.0f));
        
        // Should have reasonable ambient values
        REQUIRE(data.ambientColor.r == Approx(0.2f));
        REQUIRE(data.ambientColor.g == Approx(0.2f));
        REQUIRE(data.ambientColor.b == Approx(0.3f));
        REQUIRE(data.ambientIntensity == Approx(0.3f));
    }
    
    SECTION("Lighting calculations") {
        LightingData data;
        
        // Ambient should contribute to final lighting
        glm::vec3 ambientContribution = data.ambientColor * data.ambientIntensity;
        REQUIRE(ambientContribution.r > 0.0f);
        REQUIRE(ambientContribution.g > 0.0f);
        REQUIRE(ambientContribution.b > 0.0f);
        
        // Directional light should have valid direction
        float dirLength = glm::length(data.directionalLight.direction);
        REQUIRE(dirLength == Approx(1.0f)); // Should be normalized
    }
}

TEST_CASE("LightingSystem Directional Light Management", "[LightingSystem][DirectionalLight]") {
    SECTION("Default directional light properties") {
        LightingData data;
        auto& dirLight = data.directionalLight;
        
        // Test default direction is normalized downward
        REQUIRE(dirLight.direction.x == Approx(0.0f));
        REQUIRE(dirLight.direction.y == Approx(-1.0f));
        REQUIRE(dirLight.direction.z == Approx(0.0f));
        
        // Verify direction is normalized
        float length = glm::length(dirLight.direction);
        REQUIRE(length == Approx(1.0f).margin(0.001f));
        
        // Test default intensity
        REQUIRE(dirLight.intensity == Approx(1.0f));
        
        // Test default color (white light)
        REQUIRE(dirLight.color.r == Approx(1.0f));
        REQUIRE(dirLight.color.g == Approx(1.0f));
        REQUIRE(dirLight.color.b == Approx(1.0f));
    }
    
    SECTION("Custom directional light creation") {
        glm::vec3 customDirection(1.0f, -1.0f, 0.5f);
        float customIntensity = 2.5f;
        glm::vec3 customColor(0.8f, 0.9f, 1.0f);
        
        DirectionalLight light(customDirection, customIntensity, customColor);
        
        // Direction should be automatically normalized
        glm::vec3 expectedDirection = glm::normalize(customDirection);
        REQUIRE(light.direction.x == Approx(expectedDirection.x));
        REQUIRE(light.direction.y == Approx(expectedDirection.y));
        REQUIRE(light.direction.z == Approx(expectedDirection.z));
        
        // Verify normalization
        float length = glm::length(light.direction);
        REQUIRE(length == Approx(1.0f).margin(0.001f));
        
        // Test custom properties preserved
        REQUIRE(light.intensity == Approx(customIntensity));
        REQUIRE(light.color.r == Approx(customColor.r));
        REQUIRE(light.color.g == Approx(customColor.g));
        REQUIRE(light.color.b == Approx(customColor.b));
        
        // Test padding is initialized
        REQUIRE(light._padding == Approx(0.0f));
    }
    
    SECTION("Directional light normalization edge cases") {
        // Test zero vector handling - glm::normalize produces NaN for zero vectors
        DirectionalLight zeroLight(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, glm::vec3(1.0f));
        // glm::normalize of zero vector produces NaN values, which is expected behavior
        REQUIRE(std::isnan(zeroLight.direction.x));
        REQUIRE(std::isnan(zeroLight.direction.y));
        REQUIRE(std::isnan(zeroLight.direction.z));
        
        // Test very small vector - should still normalize to finite values
        glm::vec3 tinyVector(1e-10f, 1e-10f, 1e-10f);
        DirectionalLight tinyLight(tinyVector, 1.0f, glm::vec3(1.0f));
        REQUIRE(std::isfinite(tinyLight.direction.x));
        REQUIRE(std::isfinite(tinyLight.direction.y));
        REQUIRE(std::isfinite(tinyLight.direction.z));
        
        // Test very large vector (should normalize to unit length)
        glm::vec3 largeVector(1000.0f, 2000.0f, 3000.0f);
        DirectionalLight largeLight(largeVector, 1.0f, glm::vec3(1.0f));
        float largeLength = glm::length(largeLight.direction);
        REQUIRE(largeLength == Approx(1.0f).margin(0.001f));
    }
}

TEST_CASE("LightingSystem Ambient Light Management", "[LightingSystem][AmbientLight]") {
    SECTION("Default ambient light properties") {
        LightingData data;
        
        // Test default ambient color (blueish tint)
        REQUIRE(data.ambientColor.r == Approx(0.2f));
        REQUIRE(data.ambientColor.g == Approx(0.2f));
        REQUIRE(data.ambientColor.b == Approx(0.3f));
        
        // Test default ambient intensity
        REQUIRE(data.ambientIntensity == Approx(0.3f));
        
        // Test that ambient contribution is reasonable
        glm::vec3 ambientContribution = data.ambientColor * data.ambientIntensity;
        REQUIRE(ambientContribution.r > 0.0f);
        REQUIRE(ambientContribution.g > 0.0f);
        REQUIRE(ambientContribution.b > 0.0f);
        REQUIRE(ambientContribution.r < 1.0f);
        REQUIRE(ambientContribution.g < 1.0f);
        REQUIRE(ambientContribution.b < 1.0f);
    }
    
    SECTION("Custom ambient light configuration") {
        LightingData data;
        
        // Test custom ambient color
        glm::vec3 customAmbientColor(0.5f, 0.3f, 0.1f); // Warm ambient
        float customAmbientIntensity = 0.8f;
        
        data.ambientColor = customAmbientColor;
        data.ambientIntensity = customAmbientIntensity;
        
        REQUIRE(data.ambientColor.r == Approx(0.5f));
        REQUIRE(data.ambientColor.g == Approx(0.3f));
        REQUIRE(data.ambientColor.b == Approx(0.1f));
        REQUIRE(data.ambientIntensity == Approx(0.8f));
        
        // Test final ambient contribution
        glm::vec3 finalAmbient = data.ambientColor * data.ambientIntensity;
        REQUIRE(finalAmbient.r == Approx(0.4f));
        REQUIRE(finalAmbient.g == Approx(0.24f));
        REQUIRE(finalAmbient.b == Approx(0.08f));
    }
    
    SECTION("Ambient light intensity ranges") {
        LightingData data;
        
        // Test minimum ambient (complete darkness)
        data.ambientColor = glm::vec3(0.0f, 0.0f, 0.0f);
        data.ambientIntensity = 0.0f;
        
        glm::vec3 darkAmbient = data.ambientColor * data.ambientIntensity;
        REQUIRE(darkAmbient.r == Approx(0.0f));
        REQUIRE(darkAmbient.g == Approx(0.0f));
        REQUIRE(darkAmbient.b == Approx(0.0f));
        
        // Test maximum ambient (full white)
        data.ambientColor = glm::vec3(1.0f, 1.0f, 1.0f);
        data.ambientIntensity = 1.0f;
        
        glm::vec3 brightAmbient = data.ambientColor * data.ambientIntensity;
        REQUIRE(brightAmbient.r == Approx(1.0f));
        REQUIRE(brightAmbient.g == Approx(1.0f));
        REQUIRE(brightAmbient.b == Approx(1.0f));
    }
}

TEST_CASE("LightingSystem Memory Layout Validation", "[LightingSystem][Memory]") {
    SECTION("DirectionalLight structure alignment") {
        DirectionalLight light;
        
        // Test that DirectionalLight has proper GPU alignment
        // Should be at least 32 bytes (3 vec3 + 1 float + padding)
        REQUIRE(sizeof(DirectionalLight) >= 32);
        
        // Test individual member sizes
        REQUIRE(sizeof(light.direction) == 12);  // vec3 = 3 * float
        REQUIRE(sizeof(light.intensity) == 4);   // float
        REQUIRE(sizeof(light.color) == 12);      // vec3 = 3 * float  
        REQUIRE(sizeof(light._padding) == 4);    // float padding
        
        // Verify memory layout is predictable
        char* basePtr = reinterpret_cast<char*>(&light);
        char* directionPtr = reinterpret_cast<char*>(&light.direction);
        char* intensityPtr = reinterpret_cast<char*>(&light.intensity);
        char* colorPtr = reinterpret_cast<char*>(&light.color);
        char* paddingPtr = reinterpret_cast<char*>(&light._padding);
        
        REQUIRE(directionPtr == basePtr);
        REQUIRE(intensityPtr == basePtr + 12);
        REQUIRE(colorPtr == basePtr + 16);
        REQUIRE(paddingPtr == basePtr + 28);
    }
    
    SECTION("LightingData structure alignment") {
        LightingData data;
        
        // Test total structure size for GPU buffer compatibility
        size_t totalSize = sizeof(LightingData);
        REQUIRE(totalSize >= sizeof(DirectionalLight) + 16); // DirectionalLight + ambient data
        
        // Test that structure is properly aligned for Vulkan uniform buffers
        // Most GPUs require 256-byte alignment, but we test for basic alignment
        REQUIRE(totalSize % 4 == 0); // At least 4-byte aligned
        
        // Test ambient data layout
        REQUIRE(sizeof(data.ambientColor) == 12);    // vec3
        REQUIRE(sizeof(data.ambientIntensity) == 4); // float
        
        // Verify memory offsets are predictable
        char* basePtr = reinterpret_cast<char*>(&data);
        char* dirLightPtr = reinterpret_cast<char*>(&data.directionalLight);
        char* ambientColorPtr = reinterpret_cast<char*>(&data.ambientColor);
        char* ambientIntensityPtr = reinterpret_cast<char*>(&data.ambientIntensity);
        
        REQUIRE(dirLightPtr == basePtr);
        REQUIRE(ambientColorPtr == basePtr + sizeof(DirectionalLight));
        REQUIRE(ambientIntensityPtr >= ambientColorPtr + 12);
    }
}

// REMOVED: LightingSystem Interface Design test - silly validation of method existence

TEST_CASE("LightingSystem Calculations Validation", "[LightingSystem][Calculations]") {
    SECTION("Light vector calculations") {
        DirectionalLight light;
        
        // Test representative directional light orientations (reduced from 5 to 2)
        std::vector<glm::vec3> testDirections = {
            glm::vec3(0.0f, -1.0f, 0.0f),  // Straight down (noon sun)
            glm::vec3(-1.0f, -0.5f, 0.0f), // Angled west-down (sunset)
        };
        
        for (const auto& direction : testDirections) {
            DirectionalLight testLight(direction, 1.0f, glm::vec3(1.0f));
            
            // All directions should be normalized
            float length = glm::length(testLight.direction);
            REQUIRE(length == Approx(1.0f).margin(0.001f));
            
            // Direction should point "into" the surface (negative for lighting calcs)
            // This is design dependent - document the convention
            REQUIRE(std::isfinite(testLight.direction.x));
            REQUIRE(std::isfinite(testLight.direction.y));
            REQUIRE(std::isfinite(testLight.direction.z));
        }
    }
    
    SECTION("Lighting intensity calculations") {
        DirectionalLight light;
        LightingData data;
        
        // Test various intensity levels
        std::vector<float> testIntensities = {0.0f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f};
        
        for (float intensity : testIntensities) {
            data.directionalLight.intensity = intensity;
            
            // Intensity should be preserved exactly
            REQUIRE(data.directionalLight.intensity == Approx(intensity));
            
            // Calculate total light contribution
            glm::vec3 totalDirectional = data.directionalLight.color * intensity;
            glm::vec3 totalAmbient = data.ambientColor * data.ambientIntensity;
            
            // Total light should be reasonable
            REQUIRE(totalDirectional.r >= 0.0f);
            REQUIRE(totalDirectional.g >= 0.0f);
            REQUIRE(totalDirectional.b >= 0.0f);
            REQUIRE(totalAmbient.r >= 0.0f);
            REQUIRE(totalAmbient.g >= 0.0f);
            REQUIRE(totalAmbient.b >= 0.0f);
        }
    }
    
    SECTION("Color mixing calculations") {
        LightingData data;
        
        // Test different color combinations
        struct ColorTest {
            glm::vec3 dirColor;
            glm::vec3 ambientColor;
            float dirIntensity;
            float ambientIntensity;
            glm::vec3 expectedMax; // Maximum possible contribution
        };
        
        std::vector<ColorTest> colorTests = {
            // White light (typical case)
            {glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.2f, 0.2f, 0.2f), 1.0f, 0.3f, glm::vec3(1.06f, 1.06f, 1.06f)},
            // Warm sunset (edge case)
            {glm::vec3(1.0f, 0.7f, 0.3f), glm::vec3(0.3f, 0.2f, 0.1f), 0.8f, 0.4f, glm::vec3(0.92f, 0.64f, 0.28f)}
        };
        
        for (const auto& test : colorTests) {
            data.directionalLight.color = test.dirColor;
            data.directionalLight.intensity = test.dirIntensity;
            data.ambientColor = test.ambientColor;
            data.ambientIntensity = test.ambientIntensity;
            
            // Calculate contributions
            glm::vec3 dirContribution = test.dirColor * test.dirIntensity;
            glm::vec3 ambientContribution = test.ambientColor * test.ambientIntensity;
            glm::vec3 totalMax = dirContribution + ambientContribution;
            
            // Verify calculations are within expected ranges
            REQUIRE(totalMax.r == Approx(test.expectedMax.r).margin(0.01f));
            REQUIRE(totalMax.g == Approx(test.expectedMax.g).margin(0.01f));
            REQUIRE(totalMax.b == Approx(test.expectedMax.b).margin(0.01f));
        }
    }
}

TEST_CASE("LightingSystem Performance Characteristics", "[LightingSystem][Performance]") {
    SECTION("Structure creation performance") {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Create many lighting data structures
        std::vector<LightingData> lightingConfigs;
        lightingConfigs.reserve(10000);
        
        for (int i = 0; i < 10000; ++i) {
            float factor = static_cast<float>(i) / 10000.0f;
            
            LightingData data;
            data.directionalLight.direction = glm::normalize(glm::vec3(
                std::sin(factor * 3.14159f),
                -0.5f - factor * 0.5f,
                std::cos(factor * 3.14159f)
            ));
            data.directionalLight.intensity = 0.5f + factor * 2.0f;
            data.directionalLight.color = glm::vec3(
                0.8f + factor * 0.2f,
                0.9f,
                1.0f - factor * 0.3f
            );
            data.ambientColor = glm::vec3(0.1f + factor * 0.2f, 0.15f, 0.2f + factor * 0.1f);
            data.ambientIntensity = 0.2f + factor * 0.3f;
            
            lightingConfigs.push_back(data);
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // Creating 10000 lighting configurations should be fast (under 50ms)
        REQUIRE(duration.count() < 50);
        REQUIRE(lightingConfigs.size() == 10000);
        
        // Verify first and last configurations
        REQUIRE(lightingConfigs[0].directionalLight.intensity == Approx(0.5f));
        REQUIRE(lightingConfigs[9999].directionalLight.intensity == Approx(2.4999f).margin(0.001f));
    }
    
    SECTION("Memory usage efficiency") {
        // Test memory footprint of lighting structures
        DirectionalLight light;
        LightingData data;
        
        // DirectionalLight should be compact but aligned
        REQUIRE(sizeof(DirectionalLight) <= 64);  // Reasonable upper bound
        REQUIRE(sizeof(DirectionalLight) >= 32);  // Minimum for GPU alignment
        
        // LightingData should be efficient for uniform buffers
        REQUIRE(sizeof(LightingData) <= 128);     // Reasonable upper bound
        REQUIRE(sizeof(LightingData) >= 48);      // Minimum for content
        
        // Test that many lighting configs use reasonable memory
        std::vector<LightingData> manyConfigs(1000);
        size_t totalMemory = manyConfigs.size() * sizeof(LightingData);
        
        // 1000 lighting configs should use less than 128KB
        REQUIRE(totalMemory < 128 * 1024);
        
        // Memory usage should be predictable
        REQUIRE(totalMemory == 1000 * sizeof(LightingData));
    }
    
    SECTION("Calculation performance") {
        LightingData data;
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Perform many lighting calculations
        float totalBrightness = 0.0f;
        for (int i = 0; i < 100000; ++i) {
            // Simulate typical lighting calculations
            glm::vec3 dirContribution = data.directionalLight.color * data.directionalLight.intensity;
            glm::vec3 ambientContribution = data.ambientColor * data.ambientIntensity;
            glm::vec3 total = dirContribution + ambientContribution;
            
            // Calculate total brightness (prevents optimization)
            totalBrightness += total.r + total.g + total.b;
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // 100000 lighting calculations should be very fast (under 10ms)
        REQUIRE(duration.count() < 10);
        REQUIRE(totalBrightness > 0.0f); // Prevent optimization
    }
}

TEST_CASE("LightingSystem Edge Cases and Error Handling", "[LightingSystem][EdgeCases]") {
    SECTION("Extreme intensity values") {
        DirectionalLight light;
        
        // Test very small intensity
        light.intensity = 1e-10f;
        REQUIRE(light.intensity == Approx(1e-10f));
        REQUIRE(std::isfinite(light.intensity));
        
        // Test very large intensity
        light.intensity = 1e10f;
        REQUIRE(light.intensity == Approx(1e10f));
        REQUIRE(std::isfinite(light.intensity));
        
        // Test negative intensity (could be valid for some lighting models)
        light.intensity = -1.0f;
        REQUIRE(light.intensity == Approx(-1.0f));
        REQUIRE(std::isfinite(light.intensity));
        
        // Test zero intensity
        light.intensity = 0.0f;
        REQUIRE(light.intensity == Approx(0.0f));
        
        // Test infinity and NaN handling
        light.intensity = std::numeric_limits<float>::infinity();
        REQUIRE(!std::isfinite(light.intensity));
        
        light.intensity = std::numeric_limits<float>::quiet_NaN();
        REQUIRE(std::isnan(light.intensity));
    }
    
    SECTION("Extreme color values") {
        DirectionalLight light;
        
        // Test color values outside [0,1] range
        light.color = glm::vec3(-1.0f, 2.0f, 100.0f);
        REQUIRE(light.color.r == Approx(-1.0f));
        REQUIRE(light.color.g == Approx(2.0f));
        REQUIRE(light.color.b == Approx(100.0f));
        
        // Test very small color values
        light.color = glm::vec3(1e-10f, 1e-10f, 1e-10f);
        REQUIRE(std::isfinite(light.color.r));
        REQUIRE(std::isfinite(light.color.g));
        REQUIRE(std::isfinite(light.color.b));
        
        // Test NaN color values
        light.color = glm::vec3(
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::quiet_NaN()
        );
        REQUIRE(std::isnan(light.color.r));
        REQUIRE(std::isnan(light.color.g));
        REQUIRE(std::isnan(light.color.b));
    }
    
    SECTION("Complex lighting scenarios") {
        LightingData data;
        
        // Test mixed extreme values
        data.directionalLight.direction = glm::normalize(glm::vec3(1e-5f, -1e-5f, 1e-5f));
        data.directionalLight.intensity = 1000.0f;
        data.directionalLight.color = glm::vec3(0.001f, 0.001f, 0.001f);
        data.ambientColor = glm::vec3(0.9f, 0.9f, 0.9f);
        data.ambientIntensity = 0.001f;
        
        // Calculate contributions
        glm::vec3 dirContribution = data.directionalLight.color * data.directionalLight.intensity;
        glm::vec3 ambientContribution = data.ambientColor * data.ambientIntensity;
        
        // Results should be mathematically correct even with extreme values
        REQUIRE(dirContribution.r == Approx(1.0f));
        REQUIRE(dirContribution.g == Approx(1.0f));
        REQUIRE(dirContribution.b == Approx(1.0f));
        
        REQUIRE(ambientContribution.r == Approx(0.0009f).margin(0.0001f));
        REQUIRE(ambientContribution.g == Approx(0.0009f).margin(0.0001f));
        REQUIRE(ambientContribution.b == Approx(0.0009f).margin(0.0001f));
    }
}

// Note: Full LightingSystem functional tests requiring actual Vulkan context
// and ResourceManager integration should be in integration test suite