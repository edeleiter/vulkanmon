#include "ModelLoader.h"
#include "AssetManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <unordered_map>
#include <chrono>

namespace VulkanMon {

// ============================================================================
// ModelVertex Implementation
// ============================================================================

VkVertexInputBindingDescription ModelVertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(ModelVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> ModelVertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);

    // Position attribute
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(ModelVertex, position);

    // Normal attribute
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(ModelVertex, normal);

    // Texture coordinate attribute
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(ModelVertex, texCoords);

    // Color attribute
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(ModelVertex, color);

    return attributeDescriptions;
}

// ============================================================================
// ModelLoader Implementation
// ============================================================================

ModelLoader::ModelLoader(std::shared_ptr<ResourceManager> resourceManager, 
                        std::shared_ptr<AssetManager> assetManager)
    : resourceManager_(resourceManager)
    , assetManager_(assetManager) {
    
    VKMON_INFO("ModelLoader initialized with Assimp support");
}

std::unique_ptr<Model> ModelLoader::loadModel(const std::string& filename) {
    std::string fullPath = assetManager_->getFullAssetPath(AssetType::MODEL, filename);
    return loadModelFromFile(fullPath);
}

std::unique_ptr<Model> ModelLoader::loadModelFromFile(const std::string& fullPath) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    VKMON_INFO("Loading 3D model: " + fullPath);
    
    // Load model with Assimp
    const aiScene* scene = importer_.ReadFile(fullPath, getAssimpFlags());
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string error = "Assimp failed to load model: " + std::string(importer_.GetErrorString());
        VKMON_ERROR(error);
        throw std::runtime_error(error);
    }
    
    // Extract directory for texture loading
    std::string directory = fullPath.substr(0, fullPath.find_last_of('/'));
    
    // Process the loaded scene
    auto model = processAssimpScene(scene, directory);
    model->filename = fullPath;
    
    // Performance logging
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(endTime - startTime);
    
    VKMON_INFO("Model loaded successfully: " + fullPath);
    VKMON_INFO("  Meshes: " + std::to_string(model->meshCount()));
    VKMON_INFO("  Vertices: " + std::to_string(model->totalVertices())); 
    VKMON_INFO("  Triangles: " + std::to_string(model->totalTriangles()));
    VKMON_PERF("Model loading", duration.count());
    
    totalModelsLoaded_++;
    totalMeshesCreated_ += model->meshCount();
    totalVerticesProcessed_ += model->totalVertices();
    
    return model;
}

std::unique_ptr<Model> ModelLoader::processAssimpScene(const aiScene* scene, const std::string& directory) {
    auto model = std::make_unique<Model>("");
    model->directory = directory;
    
    // Process all meshes in the scene
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* assimpMesh = scene->mMeshes[i];
        auto mesh = processAssimpMesh(assimpMesh, scene, directory);
        if (mesh) {
            model->meshes.push_back(std::move(mesh));
        }
    }
    
    return model;
}

std::unique_ptr<Mesh> ModelLoader::processAssimpMesh(aiMesh* assimpMesh, const aiScene* scene, 
                                                    const std::string& directory) {
    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;
    
    VKMON_DEBUG("Processing mesh: " + std::string(assimpMesh->mName.C_Str()) + 
               " (" + std::to_string(assimpMesh->mNumVertices) + " vertices, " +
               std::to_string(assimpMesh->mNumFaces) + " faces)");
    
    // Process vertices
    for (unsigned int i = 0; i < assimpMesh->mNumVertices; i++) {
        ModelVertex vertex{};
        
        // Position
        vertex.position = assimpToGlm(assimpMesh->mVertices[i]);
        
        // Normal
        if (assimpMesh->HasNormals()) {
            vertex.normal = assimpToGlm(assimpMesh->mNormals[i]);
        } else {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f); // Default up normal
        }
        
        // Texture coordinates (use first UV channel)
        if (assimpMesh->mTextureCoords[0]) {
            vertex.texCoords = assimpToGlm(aiVector2D(assimpMesh->mTextureCoords[0][i].x, 
                                                     assimpMesh->mTextureCoords[0][i].y));
            // Flip Y coordinate if needed
            if (flipUVs_) {
                vertex.texCoords.y = 1.0f - vertex.texCoords.y;
            }
        } else {
            vertex.texCoords = glm::vec2(0.0f, 0.0f);
        }
        
        // Vertex colors (use first color channel if available)
        if (assimpMesh->mColors[0]) {
            vertex.color = glm::vec3(assimpMesh->mColors[0][i].r,
                                   assimpMesh->mColors[0][i].g,
                                   assimpMesh->mColors[0][i].b);
        } else {
            vertex.color = glm::vec3(1.0f); // Default white
        }
        
        vertices.push_back(vertex);
    }
    
    // Process indices
    for (unsigned int i = 0; i < assimpMesh->mNumFaces; i++) {
        aiFace face = assimpMesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
    
    // Process material
    Material material;
    if (assimpMesh->mMaterialIndex >= 0) {
        aiMaterial* assimpMaterial = scene->mMaterials[assimpMesh->mMaterialIndex];
        material = loadMaterial(assimpMaterial, directory);
    }
    
    // Create mesh
    auto mesh = std::make_unique<Mesh>(std::move(vertices), std::move(indices), std::move(material));
    
    // Create Vulkan resources
    createMeshBuffers(*mesh);
    
    return mesh;
}

Material ModelLoader::loadMaterial(aiMaterial* mat, const std::string& directory) {
    Material material;
    
    // Get material name
    aiString name;
    if (mat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
        material.name = name.C_Str();
    }
    
    // Load diffuse color
    aiColor3D color(0.0f, 0.0f, 0.0f);
    if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
        material.diffuse = glm::vec3(color.r, color.g, color.b);
    }
    
    // Load specular color
    if (mat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
        material.specular = glm::vec3(color.r, color.g, color.b);
    }
    
    // Load ambient color
    if (mat->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
        material.ambient = glm::vec3(color.r, color.g, color.b);
    }
    
    // Load shininess
    float shininess;
    if (mat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
        material.shininess = shininess;
    }
    
    // Load diffuse texture
    if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString path;
        if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
            material.diffuseTexture = path.C_Str();
        }
    }
    
    // Load normal texture
    if (mat->GetTextureCount(aiTextureType_HEIGHT) > 0) {
        aiString path;
        if (mat->GetTexture(aiTextureType_HEIGHT, 0, &path) == AI_SUCCESS) {
            material.normalTexture = path.C_Str();
        }
    }
    
    // Load specular texture
    if (mat->GetTextureCount(aiTextureType_SPECULAR) > 0) {
        aiString path;
        if (mat->GetTexture(aiTextureType_SPECULAR, 0, &path) == AI_SUCCESS) {
            material.specularTexture = path.C_Str();
        }
    }
    
    VKMON_DEBUG("Material loaded: " + material.name + 
               " (diffuse: " + material.diffuseTexture + ")");
    
    return material;
}

std::unique_ptr<Mesh> ModelLoader::createMesh(const std::vector<ModelVertex>& vertices, 
                                             const std::vector<uint32_t>& indices,
                                             const Material& material) {
    auto mesh = std::make_unique<Mesh>(vertices, indices, material);
    createMeshBuffers(*mesh);
    totalMeshesCreated_++;
    totalVerticesProcessed_ += vertices.size();
    return mesh;
}

void ModelLoader::createMeshBuffers(Mesh& mesh) {
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        VKMON_WARNING("Attempting to create buffers for empty mesh");
        return;
    }
    
    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(mesh.vertices[0]) * mesh.vertices.size();
    mesh.vertexBuffer = resourceManager_->createBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "mesh_vertices_" + mesh.material.name
    );
    
    // Copy vertex data
    mesh.vertexBuffer->updateData(mesh.vertices.data(), vertexBufferSize);
    
    // Create index buffer
    VkDeviceSize indexBufferSize = sizeof(mesh.indices[0]) * mesh.indices.size();
    mesh.indexBuffer = resourceManager_->createBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "mesh_indices_" + mesh.material.name
    );
    
    // Copy index data
    mesh.indexBuffer->updateData(mesh.indices.data(), indexBufferSize);
    
    VKMON_RESOURCE("Mesh", "created", mesh.material.name + " (" + 
                  std::to_string(mesh.vertices.size()) + " verts, " +
                  std::to_string(mesh.indices.size()) + " indices)");
}

// ============================================================================
// Utility Methods
// ============================================================================

unsigned int ModelLoader::getAssimpFlags() const {
    unsigned int flags = 0;
    
    // Core flags
    if (triangulate_) flags |= aiProcess_Triangulate;
    if (generateNormals_) flags |= aiProcess_GenNormals;
    if (flipUVs_) flags |= aiProcess_FlipUVs;
    
    // Basic optimization flags
    flags |= aiProcess_JoinIdenticalVertices;
    
    if (optimizeMeshes_) {
        flags |= aiProcess_OptimizeMeshes;
    }
    
    return flags;
}

glm::vec3 ModelLoader::assimpToGlm(const aiVector3D& vec) const {
    return glm::vec3(vec.x, vec.y, vec.z);
}

glm::vec2 ModelLoader::assimpToGlm(const aiVector2D& vec) const {
    return glm::vec2(vec.x, vec.y);
}

std::vector<std::string> ModelLoader::getSupportedFormats() {
    return {
        ".obj", ".fbx", ".dae", ".3ds", ".blend", ".ase", ".ifc", ".xgl", ".zgl",
        ".ply", ".lwo", ".lws", ".lxo", ".stl", ".x", ".ac", ".ms3d", ".cob",
        ".scn", ".mesh.xml", ".irrmesh", ".irr", ".mdl", ".md2", ".md3", ".pk3",
        ".mdc", ".md5", ".smd", ".vta", ".ogex", ".3d", ".b3d", ".q3d", ".q3s",
        ".nff", ".off", ".raw", ".ter", ".mdx", ".hmp", ".ndo", ".glb", ".gltf"
    };
}

bool ModelLoader::isFormatSupported(const std::string& extension) const {
    auto formats = getSupportedFormats();
    return std::find(formats.begin(), formats.end(), extension) != formats.end();
}

void ModelLoader::printLoadingSummary() const {
    VKMON_INFO("\n=== MODEL LOADER SUMMARY ===");
    VKMON_INFO("Models Loaded: " + std::to_string(totalModelsLoaded_));
    VKMON_INFO("Meshes Created: " + std::to_string(totalMeshesCreated_));
    VKMON_INFO("Vertices Processed: " + std::to_string(totalVerticesProcessed_));
    
    auto formats = getSupportedFormats();
    VKMON_INFO("Supported Formats: " + std::to_string(formats.size()));
    VKMON_INFO("============================");
}

// ============================================================================
// Test Model Creation (for development/testing)
// ============================================================================

std::unique_ptr<Model> ModelLoader::createTestCube() {
    std::vector<ModelVertex> vertices = {
        // Front face
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 0.0f}},
        
        // Back face
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {0.5f, 0.5f, 0.5f}},
    };
    
    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0,  // Front face
        4, 5, 6, 6, 7, 4,  // Back face
        5, 0, 3, 3, 6, 5,  // Left face
        1, 4, 7, 7, 2, 1,  // Right face
        3, 2, 7, 7, 6, 3,  // Top face
        5, 4, 1, 1, 0, 5   // Bottom face
    };
    
    Material material("test_cube");
    material.diffuse = glm::vec3(0.8f, 0.8f, 0.8f);
    
    auto model = std::make_unique<Model>("test_cube.obj");
    model->meshes.push_back(createMesh(vertices, indices, material));
    
    return model;
}

std::unique_ptr<Model> ModelLoader::createTestPlane() {
    std::vector<ModelVertex> vertices = {
        {{-1.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {{ 1.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {{ 1.0f, 0.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
        {{-1.0f, 0.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
    };
    
    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0
    };
    
    Material material("test_plane");
    material.diffuse = glm::vec3(0.6f, 0.8f, 0.6f);
    
    auto model = std::make_unique<Model>("test_plane.obj");
    model->meshes.push_back(createMesh(vertices, indices, material));
    
    return model;
}

} // namespace VulkanMon