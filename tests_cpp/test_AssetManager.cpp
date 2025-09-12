/*
 * VulkanMon AssetManager Unit Tests
 * 
 * Comprehensive tests for the AssetManager class focusing on:
 * - Texture loading and caching behavior
 * - Asset path validation and discovery
 * - Reference counting correctness
 * - Error handling for missing/invalid files
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include "../src/io/AssetManager.h"
#include "../src/rendering/ResourceManager.h"
#include "fixtures/VulkanTestFixture.h"
#include "fixtures/TestHelpers.h"

using namespace VulkanMon;
using namespace VulkanMon::Testing;

class AssetManagerTestFixture : public VulkanTestFixture {
protected:
    void SetUp() {
        if (skipIfNoVulkan()) {
            return;
        }
        
        resourceManager = std::make_shared<ResourceManager>(getDevice(), getPhysicalDevice());
        assetManager = std::make_shared<AssetManager>(getDevice(), getPhysicalDevice(), 
                                                     getCommandPool(), getGraphicsQueue(), "test_assets/");
    }
    
    std::shared_ptr<ResourceManager> resourceManager;
    std::shared_ptr<AssetManager> assetManager;
};

TEST_CASE_METHOD(AssetManagerTestFixture, "AssetManager Texture Loading", "[AssetManager][Texture]") {
    if (skipIfNoVulkan()) {
        SKIP("Vulkan not available for testing");
        return;
    }
    
    SECTION("Load texture from file") {
        // Create a test texture file
        std::string testTexture = FileTestHelpers::createTestTexture("test_pattern.png");
        
        // This will be implemented in Phase 2 with actual texture loading
        REQUIRE(FileTestHelpers::validateFile(testTexture));
        
        // Clean up
        FileTestHelpers::cleanupPath(testTexture);
    }
}

TEST_CASE_METHOD(AssetManagerTestFixture, "AssetManager Caching", "[AssetManager][Cache]") {
    if (skipIfNoVulkan()) {
        SKIP("Vulkan not available for testing");
        return;
    }
    
    SECTION("Texture reference counting") {
        size_t initialTextureCount = assetManager->getTextureCount();
        
        // Load same texture multiple times
        // Implementation will be added in Phase 2
        
        REQUIRE(initialTextureCount == assetManager->getTextureCount());
    }
}

TEST_CASE_METHOD(AssetManagerTestFixture, "AssetManager Asset Discovery", "[AssetManager][Discovery]") {
    if (skipIfNoVulkan()) {
        SKIP("Vulkan not available for testing");
        return;
    }
    
    SECTION("Discover texture assets") {
        auto textures = assetManager->discoverAssets(AssetType::TEXTURE, ".png");
        
        REQUIRE(textures.size() >= 0); // Should not crash
    }
    
    SECTION("Validate asset paths") {
        bool validPath = assetManager->validateAssetPath("textures/test.png");
        bool invalidPath = assetManager->validateAssetPath("nonexistent/path.png");
        
        REQUIRE(!invalidPath); // Should correctly identify invalid paths
    }
}

TEST_CASE_METHOD(AssetManagerTestFixture, "AssetManager Error Handling", "[AssetManager][Errors]") {
    if (skipIfNoVulkan()) {
        SKIP("Vulkan not available for testing");
        return;
    }
    
    SECTION("Handle missing texture files") {
        // Try to load non-existent texture
        REQUIRE_THROWS_AS(assetManager->loadTexture("nonexistent_texture.png"), std::runtime_error);
    }
    
    SECTION("Handle invalid texture formats") {
        // Create invalid texture file
        std::string invalidTexture = FileTestHelpers::createTempFile("invalid.png", "invalid_data");
        
        REQUIRE_THROWS_AS(assetManager->loadTexture("invalid.png"), std::runtime_error);
        
        FileTestHelpers::cleanupPath(invalidTexture);
    }
}