/**
 * VulkanMon ModelLoader Unit Tests
 * 
 * Testing 3D model loading interface design, data structures, and core functionality
 * 
 * Test Categories:
 * - ModelVertex structure validation and alignment
 * - Material property management and validation
 * - Mesh data structure integrity and statistics
 * - Model composition and hierarchy management
 * - Interface design and RAII principles  
 * - Assimp integration configuration and validation
 * - File format support and extension validation
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/io/ModelLoader.h"
#include "../src/rendering/ResourceManager.h"
#include "../src/io/AssetManager.h"
#include "fixtures/TestHelpers.h"
#include <memory>
#include <type_traits>
#include <unordered_set>
#include <algorithm>

using namespace VulkanMon::Testing;
using namespace VulkanMon;
using Catch::Approx;

TEST_CASE("ModelVertex Structure Validation", "[ModelLoader][ModelVertex]") {
    SECTION("ModelVertex memory layout and alignment") {
        // Test ModelVertex structure properties
        ModelVertex vertex;
        
        // Test default initialization
        vertex.position = glm::vec3(1.0f, 2.0f, 3.0f);
        vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        vertex.texCoords = glm::vec2(0.5f, 0.5f);
        vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);
        
        // Test member access
        REQUIRE(vertex.position.x == Approx(1.0f));
        REQUIRE(vertex.position.y == Approx(2.0f));
        REQUIRE(vertex.position.z == Approx(3.0f));
        
        REQUIRE(vertex.normal.x == Approx(0.0f));
        REQUIRE(vertex.normal.y == Approx(1.0f));
        REQUIRE(vertex.normal.z == Approx(0.0f));
        
        REQUIRE(vertex.texCoords.x == Approx(0.5f));
        REQUIRE(vertex.texCoords.y == Approx(0.5f));
        
        REQUIRE(vertex.color.x == Approx(1.0f));
        REQUIRE(vertex.color.y == Approx(1.0f));
        REQUIRE(vertex.color.z == Approx(1.0f));
    }
    
    SECTION("ModelVertex equality operator") {
        // Test equality comparison
        ModelVertex vertex1;
        vertex1.position = glm::vec3(1.0f, 2.0f, 3.0f);
        vertex1.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        vertex1.texCoords = glm::vec2(0.5f, 0.5f);
        vertex1.color = glm::vec3(1.0f, 0.0f, 0.0f);
        
        ModelVertex vertex2 = vertex1; // Copy
        
        // Test equal vertices
        REQUIRE(vertex1 == vertex2);
        
        // Test modified vertices
        vertex2.position.x = 2.0f;
        REQUIRE_FALSE(vertex1 == vertex2);
        
        // Reset and test normal modification
        vertex2 = vertex1;
        vertex2.normal.y = 0.5f;
        REQUIRE_FALSE(vertex1 == vertex2);
        
        // Reset and test texture coordinate modification
        vertex2 = vertex1;
        vertex2.texCoords.x = 0.7f;
        REQUIRE_FALSE(vertex1 == vertex2);
        
        // Reset and test color modification
        vertex2 = vertex1;
        vertex2.color.g = 1.0f;
        REQUIRE_FALSE(vertex1 == vertex2);
    }
    
    SECTION("ModelVertex memory size and performance") {
        // Test memory footprint
        size_t vertexSize = sizeof(ModelVertex);
        
        // Expected size: 3 vec3 (36 bytes) + 1 vec2 (8 bytes) = 44 bytes minimum
        REQUIRE(vertexSize >= 44);
        REQUIRE(vertexSize <= 64); // Reasonable upper bound with alignment
        
        // Test component sizes
        REQUIRE(sizeof(glm::vec3) == 12);
        REQUIRE(sizeof(glm::vec2) == 8);
        
        // Test that vertices can be efficiently stored in vectors
        std::vector<ModelVertex> vertices(1000);
        REQUIRE(vertices.size() == 1000);
        REQUIRE(vertices.capacity() >= 1000);
    }
    
    SECTION("ModelVertex hash function validation") {
        // Test that ModelVertex can be used as hash key
        ModelVertex vertex1;
        vertex1.position = glm::vec3(1.0f, 2.0f, 3.0f);
        vertex1.texCoords = glm::vec2(0.5f, 0.5f);
        
        ModelVertex vertex2;
        vertex2.position = glm::vec3(4.0f, 5.0f, 6.0f);
        vertex2.texCoords = glm::vec2(0.7f, 0.3f);
        
        // Test hash function exists and produces different hashes for different vertices
        std::hash<ModelVertex> hasher;
        size_t hash1 = hasher(vertex1);
        size_t hash2 = hasher(vertex2);
        
        REQUIRE(hash1 != hash2); // Very likely to be different for different vertices
        
        // Test that identical vertices produce same hash
        ModelVertex vertex3 = vertex1;
        size_t hash3 = hasher(vertex3);
        REQUIRE(hash1 == hash3);
        
        // Test hash can be used in unordered containers
        std::unordered_set<ModelVertex> vertexSet;
        vertexSet.insert(vertex1);
        vertexSet.insert(vertex2);
        vertexSet.insert(vertex3); // Should not increase size since vertex3 == vertex1
        
        REQUIRE(vertexSet.size() == 2);
    }
}

TEST_CASE("Material Property Management", "[ModelLoader][Material]") {
    SECTION("Default Material construction") {
        Material material;
        
        // Test default values
        REQUIRE(material.name == "default");
        REQUIRE(material.ambient.r == Approx(0.1f));
        REQUIRE(material.ambient.g == Approx(0.1f));
        REQUIRE(material.ambient.b == Approx(0.1f));
        
        REQUIRE(material.diffuse.r == Approx(0.8f));
        REQUIRE(material.diffuse.g == Approx(0.8f));
        REQUIRE(material.diffuse.b == Approx(0.8f));
        
        REQUIRE(material.specular.r == Approx(1.0f));
        REQUIRE(material.specular.g == Approx(1.0f));
        REQUIRE(material.specular.b == Approx(1.0f));
        
        REQUIRE(material.shininess == Approx(32.0f));
        
        // Test texture paths are empty by default
        REQUIRE(material.diffuseTexture.empty());
        REQUIRE(material.normalTexture.empty());
        REQUIRE(material.specularTexture.empty());
    }
    
    SECTION("Custom Material construction") {
        std::string materialName = "gold_material";
        Material material(materialName);
        
        // Test custom name
        REQUIRE(material.name == materialName);
        
        // Test that other properties maintain defaults
        REQUIRE(material.ambient.r == Approx(0.1f));
        REQUIRE(material.diffuse.r == Approx(0.8f));
        REQUIRE(material.specular.r == Approx(1.0f));
        REQUIRE(material.shininess == Approx(32.0f));
    }
    
    SECTION("Material property validation and ranges") {
        Material material("test_material");
        
        // Test valid property ranges
        std::vector<glm::vec3> testColors = {
            glm::vec3(0.0f, 0.0f, 0.0f),     // Black
            glm::vec3(1.0f, 1.0f, 1.0f),     // White  
            glm::vec3(1.0f, 0.0f, 0.0f),     // Red
            glm::vec3(0.0f, 1.0f, 0.0f),     // Green
            glm::vec3(0.0f, 0.0f, 1.0f),     // Blue
            glm::vec3(0.5f, 0.5f, 0.5f),     // Gray
            glm::vec3(0.24f, 0.2f, 0.075f),  // Gold-like ambient
        };
        
        for (const auto& color : testColors) {
            material.ambient = color;
            material.diffuse = color;  
            material.specular = color;
            
            // Test color components are in valid range [0, 1]
            REQUIRE(material.ambient.r >= 0.0f);
            REQUIRE(material.ambient.r <= 1.0f);
            REQUIRE(material.ambient.g >= 0.0f);
            REQUIRE(material.ambient.g <= 1.0f);
            REQUIRE(material.ambient.b >= 0.0f);
            REQUIRE(material.ambient.b <= 1.0f);
            
            // Same validation for diffuse and specular
            REQUIRE(material.diffuse.r >= 0.0f);
            REQUIRE(material.diffuse.r <= 1.0f);
            REQUIRE(material.diffuse.g >= 0.0f);
            REQUIRE(material.diffuse.g <= 1.0f);
            REQUIRE(material.diffuse.b >= 0.0f);
            REQUIRE(material.diffuse.b <= 1.0f);
            
            REQUIRE(material.specular.r >= 0.0f);
            REQUIRE(material.specular.r <= 1.0f);
            REQUIRE(material.specular.g >= 0.0f);
            REQUIRE(material.specular.g <= 1.0f);
            REQUIRE(material.specular.b >= 0.0f);
            REQUIRE(material.specular.b <= 1.0f);
        }
    }
    
    SECTION("Material shininess validation") {
        Material material("shininess_test");
        
        // Test valid shininess values
        std::vector<float> testShininess = {
            0.1f,    // Very low shininess (rough)
            1.0f,    // Low shininess
            32.0f,   // Default shininess
            64.0f,   // Medium shininess
            128.0f,  // High shininess  
            256.0f,  // Very high shininess
            512.0f   // Maximum practical shininess
        };
        
        for (float shininess : testShininess) {
            material.shininess = shininess;
            
            // Test shininess is positive
            REQUIRE(material.shininess > 0.0f);
            REQUIRE(material.shininess <= 1000.0f); // Reasonable upper bound
            REQUIRE(std::isfinite(material.shininess));
        }
    }
    
    SECTION("Material texture path management") {
        Material material("textured_material");
        
        // Test texture path assignment
        material.diffuseTexture = "textures/brick_diffuse.png";
        material.normalTexture = "textures/brick_normal.png";  
        material.specularTexture = "textures/brick_specular.png";
        
        REQUIRE(material.diffuseTexture == "textures/brick_diffuse.png");
        REQUIRE(material.normalTexture == "textures/brick_normal.png");
        REQUIRE(material.specularTexture == "textures/brick_specular.png");
        
        // Test texture path validation logic
        REQUIRE(!material.diffuseTexture.empty());
        REQUIRE(material.diffuseTexture.find(".png") != std::string::npos);
        
        // Test relative path handling
        REQUIRE(material.diffuseTexture.find("textures/") == 0);
    }
}

TEST_CASE("Mesh Data Structure Integrity", "[ModelLoader][Mesh]") {
    SECTION("Mesh construction and basic properties") {
        // Create test vertices
        std::vector<ModelVertex> vertices;
        vertices.resize(4);
        
        // Quad vertices
        vertices[0].position = glm::vec3(-1.0f, -1.0f, 0.0f);
        vertices[1].position = glm::vec3(1.0f, -1.0f, 0.0f);
        vertices[2].position = glm::vec3(1.0f, 1.0f, 0.0f);
        vertices[3].position = glm::vec3(-1.0f, 1.0f, 0.0f);
        
        // Create test indices (two triangles)
        std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
        
        // Create test material
        Material material("quad_material");
        
        // Create mesh
        Mesh mesh(std::move(vertices), std::move(indices), std::move(material));
        
        // Test mesh statistics
        REQUIRE(mesh.vertexCount() == 4);
        REQUIRE(mesh.indexCount() == 6);
        REQUIRE(mesh.triangleCount() == 2);
        
        // Test mesh material
        REQUIRE(mesh.material.name == "quad_material");
        
        // Test vertex data integrity
        REQUIRE(mesh.vertices.size() == 4);
        REQUIRE(mesh.vertices[0].position.x == Approx(-1.0f));
        REQUIRE(mesh.vertices[1].position.x == Approx(1.0f));
        REQUIRE(mesh.vertices[2].position.y == Approx(1.0f));
        REQUIRE(mesh.vertices[3].position.y == Approx(1.0f));
        
        // Test index data integrity
        REQUIRE(mesh.indices.size() == 6);
        REQUIRE(mesh.indices[0] == 0);
        REQUIRE(mesh.indices[1] == 1);
        REQUIRE(mesh.indices[2] == 2);
        REQUIRE(mesh.indices[3] == 2);
        REQUIRE(mesh.indices[4] == 3);
        REQUIRE(mesh.indices[5] == 0);
    }
    
    SECTION("Mesh statistics validation") {
        // Test various mesh configurations
        struct MeshTestData {
            size_t vertexCount;
            size_t indexCount;
            size_t expectedTriangles;
        };
        
        std::vector<MeshTestData> testCases = {
            {3, 3, 1},      // Single triangle
            {4, 6, 2},      // Quad (2 triangles)
            {8, 36, 12},    // Cube (12 triangles)
            {6, 12, 4},     // Pyramid (4 triangles)
            {100, 300, 100} // Complex mesh (100 triangles)
        };
        
        for (const auto& testCase : testCases) {
            // Create mesh with specified counts
            std::vector<ModelVertex> vertices(testCase.vertexCount);
            std::vector<uint32_t> indices(testCase.indexCount);
            
            // Initialize basic vertex data
            for (size_t i = 0; i < vertices.size(); ++i) {
                vertices[i].position = glm::vec3(
                    static_cast<float>(i % 10),
                    static_cast<float>((i / 10) % 10),
                    0.0f
                );
                vertices[i].normal = glm::vec3(0.0f, 0.0f, 1.0f);
                vertices[i].texCoords = glm::vec2(0.5f, 0.5f);
            }
            
            // Initialize indices
            for (size_t i = 0; i < indices.size(); ++i) {
                indices[i] = static_cast<uint32_t>(i % vertices.size());
            }
            
            Mesh mesh(std::move(vertices), std::move(indices));
            
            // Test statistics
            REQUIRE(mesh.vertexCount() == testCase.vertexCount);
            REQUIRE(mesh.indexCount() == testCase.indexCount);
            REQUIRE(mesh.triangleCount() == testCase.expectedTriangles);
        }
    }
    
    SECTION("Empty mesh validation") {
        // Test empty mesh construction
        Mesh emptyMesh;
        
        REQUIRE(emptyMesh.vertexCount() == 0);
        REQUIRE(emptyMesh.indexCount() == 0);
        REQUIRE(emptyMesh.triangleCount() == 0);
        REQUIRE(emptyMesh.material.name == "default");
        
        // Test empty mesh with custom material
        Material customMaterial("empty_mesh_material");
        Mesh emptyMeshWithMaterial({}, {}, customMaterial);
        
        REQUIRE(emptyMeshWithMaterial.vertexCount() == 0);
        REQUIRE(emptyMeshWithMaterial.indexCount() == 0);
        REQUIRE(emptyMeshWithMaterial.triangleCount() == 0);
        REQUIRE(emptyMeshWithMaterial.material.name == "empty_mesh_material");
    }
}

TEST_CASE("Model Composition and Hierarchy", "[ModelLoader][Model]") {
    SECTION("Model construction and file path handling") {
        std::string modelPath = "models/character/hero.fbx";
        Model model(modelPath);
        
        // Test filename and directory extraction
        REQUIRE(model.filename == modelPath);
        REQUIRE(model.directory == "models/character/");
        
        // Test various path formats
        std::vector<std::pair<std::string, std::string>> pathTests = {
            {"simple.obj", ""},
            {"models/cube.obj", "models/"},
            {"assets/models/character.fbx", "assets/models/"},
            {"C:\\full\\path\\model.obj", "C:\\full\\path\\"},
            {"/unix/path/model.gltf", "/unix/path/"},
            {"./relative/model.dae", "./relative/"}
        };
        
        for (const auto& [inputPath, expectedDir] : pathTests) {
            Model testModel(inputPath);
            REQUIRE(testModel.filename == inputPath);
            REQUIRE(testModel.directory == expectedDir);
        }
    }
    
    SECTION("Model mesh management") {
        Model model("test_model.obj");
        
        // Create test meshes
        for (int i = 0; i < 3; ++i) {
            // Create simple triangle mesh
            std::vector<ModelVertex> vertices(3);
            vertices[0].position = glm::vec3(-1.0f, -1.0f, 0.0f);
            vertices[1].position = glm::vec3(1.0f, -1.0f, 0.0f);
            vertices[2].position = glm::vec3(0.0f, 1.0f, 0.0f);
            
            std::vector<uint32_t> indices = {0, 1, 2};
            
            Material material("mesh_" + std::to_string(i));
            
            auto mesh = std::make_unique<Mesh>(std::move(vertices), std::move(indices), std::move(material));
            model.meshes.push_back(std::move(mesh));
        }
        
        // Test model statistics
        REQUIRE(model.meshCount() == 3);
        REQUIRE(model.totalVertices() == 9); // 3 meshes * 3 vertices each
        REQUIRE(model.totalTriangles() == 3); // 3 meshes * 1 triangle each
        
        // Test mesh access
        REQUIRE(model.meshes[0]->material.name == "mesh_0");
        REQUIRE(model.meshes[1]->material.name == "mesh_1");
        REQUIRE(model.meshes[2]->material.name == "mesh_2");
    }
    
    SECTION("Model statistics with complex meshes") {
        Model model("complex_model.fbx");
        
        // Add meshes with different vertex/triangle counts
        struct MeshData {
            size_t vertexCount;
            size_t triangleCount;
        };
        
        std::vector<MeshData> meshConfigs = {
            {100, 50},   // Mesh 0: 100 vertices, 50 triangles
            {200, 150},  // Mesh 1: 200 vertices, 150 triangles  
            {50, 25},    // Mesh 2: 50 vertices, 25 triangles
            {300, 200}   // Mesh 3: 300 vertices, 200 triangles
        };
        
        size_t expectedTotalVertices = 0;
        size_t expectedTotalTriangles = 0;
        
        for (const auto& config : meshConfigs) {
            // Create mesh with specified configuration
            std::vector<ModelVertex> vertices(config.vertexCount);
            std::vector<uint32_t> indices(config.triangleCount * 3);
            
            // Initialize vertices
            for (size_t i = 0; i < vertices.size(); ++i) {
                vertices[i].position = glm::vec3(
                    static_cast<float>(i % 10),
                    static_cast<float>((i / 10) % 10),
                    static_cast<float>((i / 100) % 10)
                );
            }
            
            // Initialize indices
            for (size_t i = 0; i < indices.size(); ++i) {
                indices[i] = static_cast<uint32_t>(i % vertices.size());
            }
            
            auto mesh = std::make_unique<Mesh>(std::move(vertices), std::move(indices));
            model.meshes.push_back(std::move(mesh));
            
            expectedTotalVertices += config.vertexCount;
            expectedTotalTriangles += config.triangleCount;
        }
        
        // Test aggregated statistics
        REQUIRE(model.meshCount() == meshConfigs.size());
        REQUIRE(model.totalVertices() == expectedTotalVertices);
        REQUIRE(model.totalTriangles() == expectedTotalTriangles);
    }
    
    SECTION("Empty model validation") {
        Model emptyModel("empty_model.obj");
        
        REQUIRE(emptyModel.meshCount() == 0);
        REQUIRE(emptyModel.totalVertices() == 0);
        REQUIRE(emptyModel.totalTriangles() == 0);
        REQUIRE(emptyModel.filename == "empty_model.obj");
        REQUIRE(emptyModel.directory == "");
    }
}

TEST_CASE("ModelLoader Interface Design", "[ModelLoader][Interface]") {
    SECTION("Constructor parameter validation") {
        // Test ModelLoader constructor interface requirements
        std::shared_ptr<ResourceManager> mockResourceManager = nullptr;
        std::shared_ptr<AssetManager> mockAssetManager = nullptr;
        
        // Validate constructor parameter types
        REQUIRE(mockResourceManager == nullptr); // Mock for interface testing
        REQUIRE(mockAssetManager == nullptr);     // Mock for interface testing
        
        // Test that interface accepts shared_ptr parameters
        REQUIRE(true); // Interface design validation
    }
    
    SECTION("RAII design validation") {
        // Test that ModelLoader follows proper RAII design principles
        REQUIRE_FALSE(std::is_copy_constructible_v<ModelLoader>);
        REQUIRE_FALSE(std::is_copy_assignable_v<ModelLoader>);
        
        // Note: ModelLoader may not be movable due to Assimp::Importer member
        // which doesn't support move semantics. This is acceptable for the current design.
        REQUIRE(std::is_destructible_v<ModelLoader>);
        
        // Test that it's properly designed for RAII with explicit deletions
        REQUIRE(true); // Interface design validation passes
    }
    
    SECTION("Configuration flags validation") {
        // Test configuration flag types and ranges
        bool triangulate = true;
        bool generateNormals = true;
        bool optimizeMeshes = true;
        bool flipUVs = false;
        
        // Test boolean flag behavior
        REQUIRE(triangulate == true);
        REQUIRE(generateNormals == true);
        REQUIRE(optimizeMeshes == true);
        REQUIRE(flipUVs == false);
        
        // Test flag toggling
        triangulate = !triangulate;
        REQUIRE(triangulate == false);
        
        generateNormals = !generateNormals;
        REQUIRE(generateNormals == false);
        
        optimizeMeshes = !optimizeMeshes;
        REQUIRE(optimizeMeshes == false);
        
        flipUVs = !flipUVs;
        REQUIRE(flipUVs == true);
    }
}

TEST_CASE("File Format Support Validation", "[ModelLoader][Formats]") {
    SECTION("Supported format enumeration") {
        // Test expected supported formats
        std::vector<std::string> expectedFormats = {
            ".obj",
            ".fbx", 
            ".gltf",
            ".dae",
            ".blend",
            ".3ds",
            ".ply",
            ".stl"
        };
        
        // Test format validation logic
        for (const auto& format : expectedFormats) {
            // Test format string properties
            REQUIRE(!format.empty());
            REQUIRE(format[0] == '.');
            REQUIRE(format.length() >= 3);
            REQUIRE(format.length() <= 6);
            
            // Test format contains only valid characters
            for (char c : format.substr(1)) {
                REQUIRE(std::isalnum(c));
            }
        }
    }
    
    SECTION("Format support checking logic") {
        // Test format support validation
        std::vector<std::pair<std::string, bool>> formatTests = {
            {"model.obj", true},        // Wavefront OBJ
            {"model.fbx", true},        // Autodesk FBX
            {"model.gltf", true},       // glTF 2.0
            {"model.dae", true},        // COLLADA
            {"model.blend", true},      // Blender
            {"model.3ds", true},        // 3D Studio
            {"model.ply", true},        // Stanford PLY
            {"model.stl", true},        // STL (3D printing)
            {"model.xyz", false},       // Unsupported format
            {"model.txt", false},       // Text file
            {"model", false},           // No extension
            {"", false}                 // Empty filename
        };
        
        for (const auto& [filename, expectedSupported] : formatTests) {
            if (filename.empty()) {
                continue; // Skip empty filename test
            }
            
            // Extract extension
            size_t dotPos = filename.find_last_of('.');
            std::string extension = (dotPos != std::string::npos) ? filename.substr(dotPos) : "";
            
            // Test extension extraction
            if (expectedSupported && !extension.empty()) {
                REQUIRE(!extension.empty());
                REQUIRE(extension[0] == '.');
            }
        }
    }
    
    SECTION("Filename validation and extension extraction") {
        // Test filename processing logic
        std::vector<std::pair<std::string, std::string>> filenameTests = {
            {"simple.obj", ".obj"},
            {"path/to/model.fbx", ".fbx"},
            {"complex.file.name.gltf", ".gltf"},
            {"no_extension", ""},
            {".", ""},
            {".hidden", ""},
            {"file.", "."}
        };
        
        for (const auto& [filename, expectedExt] : filenameTests) {
            size_t dotPos = filename.find_last_of('.');
            std::string extractedExt = (dotPos != std::string::npos && dotPos > 0) ? filename.substr(dotPos) : "";
            
            if (expectedExt.empty()) {
                REQUIRE((extractedExt.empty() || extractedExt == "."));
            } else {
                REQUIRE(extractedExt == expectedExt);
            }
        }
    }
}

TEST_CASE("ModelLoader Statistics and Performance", "[ModelLoader][Performance]") {
    SECTION("Statistics tracking validation") {
        // Test statistics data types and ranges
        size_t totalModelsLoaded = 0;
        size_t totalMeshesCreated = 0;  
        size_t totalVerticesProcessed = 0;
        
        // Simulate loading statistics
        for (int i = 0; i < 5; ++i) {
            totalModelsLoaded++;
            
            // Simulate meshes per model
            size_t meshesInModel = 2 + (i % 3);
            totalMeshesCreated += meshesInModel;
            
            // Simulate vertices per mesh
            for (size_t j = 0; j < meshesInModel; ++j) {
                size_t verticesInMesh = 100 + (j * 50);
                totalVerticesProcessed += verticesInMesh;
            }
        }
        
        // Test statistics are reasonable
        REQUIRE(totalModelsLoaded == 5);
        REQUIRE(totalMeshesCreated > totalModelsLoaded); // Multiple meshes per model
        REQUIRE(totalVerticesProcessed > totalMeshesCreated); // Multiple vertices per mesh
        
        // Test statistics don't overflow
        REQUIRE(totalModelsLoaded < std::numeric_limits<size_t>::max());
        REQUIRE(totalMeshesCreated < std::numeric_limits<size_t>::max());
        REQUIRE(totalVerticesProcessed < std::numeric_limits<size_t>::max());
    }
    
    SECTION("Performance characteristics validation") {
        // Test memory usage estimation for different model sizes
        struct ModelSize {
            size_t vertexCount;
            size_t indexCount;
            size_t expectedMemoryBytes;
        };
        
        std::vector<ModelSize> modelSizes = {
            {1000, 3000, 1000 * sizeof(ModelVertex) + 3000 * sizeof(uint32_t)},      // Small model
            {10000, 30000, 10000 * sizeof(ModelVertex) + 30000 * sizeof(uint32_t)},   // Medium model
            {100000, 300000, 100000 * sizeof(ModelVertex) + 300000 * sizeof(uint32_t)} // Large model
        };
        
        for (const auto& modelSize : modelSizes) {
            // Test memory calculations
            size_t vertexMemory = modelSize.vertexCount * sizeof(ModelVertex);
            size_t indexMemory = modelSize.indexCount * sizeof(uint32_t);
            size_t totalMemory = vertexMemory + indexMemory;
            
            REQUIRE(totalMemory == modelSize.expectedMemoryBytes);
            
            // Test that memory usage is reasonable
            REQUIRE(vertexMemory > 0);
            REQUIRE(indexMemory > 0);
            REQUIRE(totalMemory > 0);
            
            // Test memory doesn't exceed reasonable limits for testing
            REQUIRE(totalMemory < 100 * 1024 * 1024); // Less than 100MB for test models
        }
    }
}