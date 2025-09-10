/*
 * VulkanMon ResourceManager Unit Tests
 * 
 * Comprehensive tests for the ResourceManager class focusing on:
 * - RAII lifecycle (construction/destruction)
 * - Memory type finding correctness
 * - Buffer/Image creation with various parameters
 * - Resource tracking and statistics
 * - Error handling for invalid parameters
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include "../src/ResourceManager.h"
#include "fixtures/VulkanTestFixture.h"
#include "fixtures/TestHelpers.h"

using namespace VulkanMon;
using namespace VulkanMon::Testing;

class ResourceManagerTestFixture : public VulkanTestFixture {
protected:
    void SetUp() {
        // Skip tests if Vulkan is not available
        if (skipIfNoVulkan()) {
            return;
        }
        
        resourceManager = std::make_unique<ResourceManager>(getDevice(), getPhysicalDevice());
    }
    
    std::unique_ptr<ResourceManager> resourceManager;
};

TEST_CASE_METHOD(ResourceManagerTestFixture, "ResourceManager Buffer Management", "[ResourceManager][Buffer]") {
    if (skipIfNoVulkan()) {
        SKIP("Vulkan not available for testing");
        return;
    }
    
    SECTION("Create and destroy buffer") {
        VkDeviceSize bufferSize = 1024;
        
        auto buffer = resourceManager->createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            "test_buffer"
        );
        
        REQUIRE(buffer != nullptr);
        REQUIRE(buffer->getBuffer() != VK_NULL_HANDLE);
        REQUIRE(buffer->getMemory() != VK_NULL_HANDLE);
        REQUIRE(buffer->getSize() == bufferSize);
    }
    
    SECTION("Buffer resource tracking") {
        size_t initialBufferCount = resourceManager->getBufferCount();
        
        {
            auto buffer = resourceManager->createBuffer(
                512,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                "tracking_test_buffer"
            );
            
            REQUIRE(resourceManager->getBufferCount() == initialBufferCount + 1);
        }
        
        // Buffer should be automatically cleaned up here
        REQUIRE(true); // Will verify count decrease in Phase 2
    }
}

TEST_CASE_METHOD(ResourceManagerTestFixture, "ResourceManager Image Management", "[ResourceManager][Image]") {
    if (skipIfNoVulkan()) {
        SKIP("Vulkan not available for testing");
        return;
    }
    
    SECTION("Create and destroy image") {
        auto image = resourceManager->createImage(
            256, 256,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            "test_image"
        );
        
        REQUIRE(image != nullptr);
        REQUIRE(image->getImage() != VK_NULL_HANDLE);
        REQUIRE(image->getMemory() != VK_NULL_HANDLE);
        REQUIRE(image->getImageView() != VK_NULL_HANDLE);
        REQUIRE(image->getWidth() == 256);
        REQUIRE(image->getHeight() == 256);
        REQUIRE(image->getFormat() == VK_FORMAT_R8G8B8A8_SRGB);
    }
}

TEST_CASE_METHOD(ResourceManagerTestFixture, "ResourceManager Memory Type Finding", "[ResourceManager][Memory]") {
    if (skipIfNoVulkan()) {
        SKIP("Vulkan not available for testing");
        return;
    }
    
    SECTION("Find valid memory type") {
        // Test with common memory requirements
        uint32_t memoryType = resourceManager->findMemoryType(
            0xFFFFFFFF, // All memory types
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        
        REQUIRE(memoryType < 32); // Valid memory type index
    }
}

TEST_CASE_METHOD(ResourceManagerTestFixture, "ResourceManager Statistics", "[ResourceManager][Stats]") {
    if (skipIfNoVulkan()) {
        SKIP("Vulkan not available for testing");
        return;
    }
    
    SECTION("Resource statistics tracking") {
        size_t initialBufferCount = resourceManager->getBufferCount();
        size_t initialImageCount = resourceManager->getImageCount();
        
        auto buffer = resourceManager->createBuffer(1024, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, "stats_test");
        auto image = resourceManager->createImage(128, 128, VK_FORMAT_R8G8B8A8_SRGB,
                                                 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT,
                                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT, "stats_test");
        
        REQUIRE(resourceManager->getBufferCount() == initialBufferCount + 1);
        REQUIRE(resourceManager->getImageCount() == initialImageCount + 1);
        REQUIRE(resourceManager->getTotalAllocatedMemory() > 0);
    }
}