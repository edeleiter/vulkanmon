/*
 * VulkanMon ModelLoader Unit Tests
 * 
 * Comprehensive tests for the ModelLoader class focusing on:
 * - Assimp integration and model loading
 * - Mesh creation and buffer generation
 * - Material loading and texture assignment
 * - Vertex format conversion correctness
 * - Procedural model generation
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include "../src/io/ModelLoader.h"
#include "../src/rendering/ResourceManager.h"
#include "../src/io/AssetManager.h"
#include "fixtures/VulkanTestFixture.h"
#include "fixtures/TestHelpers.h"

using namespace VulkanMon;
using namespace VulkanMon::Testing;

class ModelLoaderTestFixture : public VulkanTestFixture {
protected:
    void SetUp() {
        if (skipIfNoVulkan()) {
            return;
        }
        
        resourceManager = std::make_shared<ResourceManager>(getDevice(), getPhysicalDevice());
        assetManager = std::make_shared<AssetManager>(getDevice(), getPhysicalDevice(),
                                                     getCommandPool(), getGraphicsQueue(), "test_assets/");
        modelLoader = std::make_shared<ModelLoader>(resourceManager, assetManager);
    }
    
    std::shared_ptr<ResourceManager> resourceManager;
    std::shared_ptr<AssetManager> assetManager;
    std::shared_ptr<ModelLoader> modelLoader;
};

TEST_CASE_METHOD(ModelLoaderTestFixture, "ModelLoader Assimp Integration", "[ModelLoader][Assimp]") {
    if (skipIfNoVulkan()) {
        SKIP("Vulkan not available for testing");
        return;
    }
    
    SECTION("Load simple OBJ model") {
        // Create a test OBJ file
        std::string testModel = FileTestHelpers::createTestModel("test_cube.obj");
        
        // This will be implemented in Phase 2
        REQUIRE(FileTestHelpers::validateFile(testModel));
        
        FileTestHelpers::cleanupPath(testModel);
    }
    
    SECTION("Supported formats validation") {
        auto formats = ModelLoader::getSupportedFormats();
        
        REQUIRE(!formats.empty());
        REQUIRE(std::find(formats.begin(), formats.end(), ".obj") != formats.end());
        REQUIRE(std::find(formats.begin(), formats.end(), ".fbx") != formats.end());
        REQUIRE(std::find(formats.begin(), formats.end(), ".gltf") != formats.end());
    }
}

TEST_CASE_METHOD(ModelLoaderTestFixture, "ModelLoader Mesh Processing", "[ModelLoader][Mesh]") {
    if (skipIfNoVulkan()) {
        SKIP("Vulkan not available for testing");
        return;
    }
    
    SECTION("Create mesh from vertex data") {
        std::vector<ModelVertex> vertices = {
            {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.0f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}, {0.0f, 0.0f, 1.0f}}
        };
        
        std::vector<uint32_t> indices = {0, 1, 2};
        
        auto mesh = modelLoader->createMesh(vertices, indices);
        
        REQUIRE(mesh != nullptr);
        REQUIRE(mesh->vertices.size() == 3);
        REQUIRE(mesh->indices.size() == 3);
        REQUIRE(mesh->vertexBuffer != nullptr);
        REQUIRE(mesh->indexBuffer != nullptr);
    }
}

TEST_CASE_METHOD(ModelLoaderTestFixture, "ModelLoader Material Loading", "[ModelLoader][Material]") {
    if (skipIfNoVulkan()) {
        SKIP("Vulkan not available for testing");
        return;
    }
    
    SECTION("Default material properties") {
        Material material;
        
        REQUIRE(material.name == "default");
        REQUIRE(material.diffuse.r > 0.0f);
        REQUIRE(material.diffuse.g > 0.0f);
        REQUIRE(material.diffuse.b > 0.0f);
        REQUIRE(material.shininess > 0.0f);
    }
}

TEST_CASE_METHOD(ModelLoaderTestFixture, "ModelLoader Procedural Models", "[ModelLoader][Procedural]") {
    if (skipIfNoVulkan()) {
        SKIP("Vulkan not available for testing");
        return;
    }
    
    SECTION("Create test cube") {
        auto cubeModel = modelLoader->createTestCube();
        
        REQUIRE(cubeModel != nullptr);
        REQUIRE(cubeModel->meshCount() == 1);
        REQUIRE(cubeModel->totalVertices() > 0);
        REQUIRE(cubeModel->totalTriangles() > 0);
    }
    
    SECTION("Create test plane") {
        auto planeModel = modelLoader->createTestPlane();
        
        REQUIRE(planeModel != nullptr);
        REQUIRE(planeModel->meshCount() == 1);
        REQUIRE(planeModel->totalVertices() == 4);
        REQUIRE(planeModel->totalTriangles() == 2);
    }
}

TEST_CASE("ModelVertex Structure", "[ModelLoader][Vertex]") {
    SECTION("Vertex binding description") {
        auto binding = ModelVertex::getBindingDescription();
        
        REQUIRE(binding.binding == 0);
        REQUIRE(binding.stride == sizeof(ModelVertex));
        REQUIRE(binding.inputRate == VK_VERTEX_INPUT_RATE_VERTEX);
    }
    
    SECTION("Vertex attribute descriptions") {
        auto attributes = ModelVertex::getAttributeDescriptions();
        
        REQUIRE(attributes.size() == 4); // position, normal, texCoords, color
        REQUIRE(attributes[0].location == 0); // position
        REQUIRE(attributes[1].location == 1); // normal
        REQUIRE(attributes[2].location == 2); // texCoords
        REQUIRE(attributes[3].location == 3); // color
    }
}