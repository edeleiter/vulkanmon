#pragma once

#include "../rendering/ResourceManager.h"
#include "../utils/Logger.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>

// Assimp includes
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

/**
 * VulkanMon Model Loading System
 * 
 * Integrates with Assimp for loading 3D models following our philosophy:
 * - "Simple is Powerful" - Clear mesh representation, straightforward loading
 * - "Test, Test, Test" - Robust model validation and error handling  
 * - "Document Often" - Well-documented vertex formats and loading process
 * 
 * Features:
 * - Assimp integration for multiple 3D formats (.obj, .fbx, .gltf, .dae)
 * - Efficient vertex and index buffer creation
 * - Material loading and texture assignment
 * - Mesh optimization and validation
 * - Memory-efficient resource management
 */

namespace VulkanMon {

// Forward declarations
class ResourceManager;
class ManagedBuffer;
class AssetManager;

/**
 * Vertex structure for 3D models
 * Extended from the basic triangle vertex to support full 3D rendering
 */
struct ModelVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
    glm::vec3 color = glm::vec3(1.0f); // Default white
    
    // Vulkan vertex input description
    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    
    bool operator==(const ModelVertex& other) const {
        return position == other.position && 
               normal == other.normal && 
               texCoords == other.texCoords &&
               color == other.color;
    }
};

/**
 * Material properties for mesh rendering
 */
struct Material {
    std::string name;
    glm::vec3 ambient = glm::vec3(0.1f);
    glm::vec3 diffuse = glm::vec3(0.8f);
    glm::vec3 specular = glm::vec3(1.0f);
    float shininess = 32.0f;
    
    // Texture paths (relative to assets/textures/)
    std::string diffuseTexture;
    std::string normalTexture;
    std::string specularTexture;
    
    Material(const std::string& materialName = "default") : name(materialName) {}
};

/**
 * Mesh representation - single drawable unit
 */
struct Mesh {
    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;
    Material material;
    
    // Vulkan resources (created by ModelLoader)
    std::unique_ptr<ManagedBuffer> vertexBuffer;
    std::unique_ptr<ManagedBuffer> indexBuffer;
    
    // Mesh statistics
    size_t vertexCount() const { return vertices.size(); }
    size_t indexCount() const { return indices.size(); }
    size_t triangleCount() const { return indices.size() / 3; }
    
    Mesh() = default;
    Mesh(std::vector<ModelVertex> verts, std::vector<uint32_t> inds, Material mat = Material())
        : vertices(std::move(verts)), indices(std::move(inds)), material(std::move(mat)) {}
};

/**
 * Complete 3D model - collection of meshes
 */
struct Model {
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::string filename;
    std::string directory; // For resolving relative texture paths
    
    // Model statistics
    size_t meshCount() const { return meshes.size(); }
    size_t totalVertices() const {
        size_t total = 0;
        for (const auto& mesh : meshes) total += mesh->vertexCount();
        return total;
    }
    size_t totalTriangles() const {
        size_t total = 0;
        for (const auto& mesh : meshes) total += mesh->triangleCount();
        return total;
    }
    
    Model(const std::string& file) : filename(file) {
        // Extract directory from filename for texture loading
        size_t lastSlash = file.find_last_of("/\\");
        directory = (lastSlash != std::string::npos) ? file.substr(0, lastSlash + 1) : "";
    }
};

/**
 * ModelLoader - Assimp integration and Vulkan resource creation
 */
class ModelLoader {
public:
    explicit ModelLoader(std::shared_ptr<ResourceManager> resourceManager, 
                        std::shared_ptr<AssetManager> assetManager);
    ~ModelLoader() = default;
    
    // Move-only semantics
    ModelLoader(const ModelLoader&) = delete;
    ModelLoader& operator=(const ModelLoader&) = delete;
    ModelLoader(ModelLoader&&) = default;
    ModelLoader& operator=(ModelLoader&&) = default;
    
    // Model loading
    std::unique_ptr<Model> loadModel(const std::string& filename);
    std::unique_ptr<Model> loadModelFromFile(const std::string& fullPath);
    
    // Mesh processing
    std::unique_ptr<Mesh> createMesh(const std::vector<ModelVertex>& vertices, 
                                   const std::vector<uint32_t>& indices,
                                   const Material& material = Material());
    
    // Utility functions
    static std::vector<std::string> getSupportedFormats();
    bool isFormatSupported(const std::string& extension) const;
    
    // Configuration
    void setTriangulate(bool enable) { triangulate_ = enable; }
    void setGenerateNormals(bool enable) { generateNormals_ = enable; }
    void setOptimizeMeshes(bool enable) { optimizeMeshes_ = enable; }
    void setFlipUVs(bool enable) { flipUVs_ = enable; }
    
    // Statistics
    void printLoadingSummary() const;
    
    // Test model creation (for development/testing)
    std::unique_ptr<Model> createTestCube();
    std::unique_ptr<Model> createTestPlane();

private:
    std::shared_ptr<ResourceManager> resourceManager_;
    std::shared_ptr<AssetManager> assetManager_;
    
    // Configuration flags (for Assimp processing)
    bool triangulate_ = true;
    bool generateNormals_ = true;
    bool optimizeMeshes_ = true;
    bool flipUVs_ = false;
    
    // Statistics
    mutable size_t totalModelsLoaded_ = 0;
    mutable size_t totalMeshesCreated_ = 0;
    mutable size_t totalVerticesProcessed_ = 0;
    
    // Assimp instance for model loading
    Assimp::Importer importer_;
    
    // Assimp-specific methods
    std::unique_ptr<Model> processAssimpScene(const aiScene* scene, const std::string& directory);
    std::unique_ptr<Mesh> processAssimpMesh(aiMesh* mesh, const aiScene* scene, const std::string& directory);
    Material loadMaterial(aiMaterial* mat, const std::string& directory);
    
    // Assimp utility methods
    unsigned int getAssimpFlags() const;
    glm::vec3 assimpToGlm(const aiVector3D& vec) const;
    glm::vec2 assimpToGlm(const aiVector2D& vec) const;
    
    // Vulkan resource creation helpers
    void createMeshBuffers(Mesh& mesh);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

} // namespace VulkanMon

// Hash function for ModelVertex (for vertex deduplication)  
namespace std {
    template<> struct hash<VulkanMon::ModelVertex> {
        size_t operator()(const VulkanMon::ModelVertex& vertex) const {
            size_t h1 = std::hash<float>{}(vertex.position.x);
            size_t h2 = std::hash<float>{}(vertex.position.y);
            size_t h3 = std::hash<float>{}(vertex.position.z);
            size_t h4 = std::hash<float>{}(vertex.texCoords.x);
            size_t h5 = std::hash<float>{}(vertex.texCoords.y);
            
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
        }
    };
}