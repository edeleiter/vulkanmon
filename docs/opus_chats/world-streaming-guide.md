# World Streaming System
## Week 5-6: Building an Open World Like Pokémon Legends: Arceus

## Overview
World streaming is essential for creating the seamless open-world experience of Pokémon Legends: Arceus. This system handles terrain generation, chunk loading/unloading, LOD management, and environmental details like grass and trees.

## Core Architecture

### World Grid System

```cpp
// include/world/WorldGrid.h
#pragma once
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <thread>
#include <atomic>

namespace VulkanMon {

// World is divided into chunks
class WorldChunk {
public:
    static constexpr int CHUNK_SIZE = 64;  // 64x64 meters
    static constexpr int VERTICES_PER_SIDE = 65;  // One extra for edges
    
    struct ChunkCoord {
        int32_t x, z;
        
        bool operator==(const ChunkCoord& other) const {
            return x == other.x && z == other.z;
        }
    };
    
    struct ChunkCoordHash {
        std::size_t operator()(const ChunkCoord& coord) const {
            return std::hash<int32_t>{}(coord.x) ^ 
                   (std::hash<int32_t>{}(coord.z) << 1);
        }
    };
    
    enum LODLevel {
        LOD0 = 0,  // Full detail (closest)
        LOD1 = 1,  // Half resolution
        LOD2 = 2,  // Quarter resolution  
        LOD3 = 3,  // Eighth resolution
        LOD_COUNT = 4
    };
    
    enum ChunkState {
        Unloaded,
        Loading,
        Loaded,
        Unloading
    };
    
    ChunkCoord coord;
    ChunkState state = Unloaded;
    LODLevel currentLOD = LOD3;
    
    // Terrain data
    std::vector<float> heightmap;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<uint32_t> indices[LOD_COUNT];
    
    // Terrain layers (splatmap)
    std::vector<glm::vec4> splatMap;  // RGBA = 4 terrain textures
    
    // Vegetation
    struct VegetationInstance {
        glm::vec3 position;
        float rotation;
        float scale;
        uint8_t type;  // Grass, flower, bush, etc.
    };
    std::vector<VegetationInstance> vegetation;
    
    // Objects in this chunk
    std::vector<EntityId> entities;
    std::vector<EntityId> staticObjects;  // Trees, rocks, etc.
    
    // GPU resources
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffers[LOD_COUNT] = {};
    VkDeviceMemory memory = VK_NULL_HANDLE;
    
    // Collision mesh (simplified)
    btTriangleMesh* collisionMesh = nullptr;
    btRigidBody* rigidBody = nullptr;
    
    glm::vec3 getWorldPosition() const {
        return glm::vec3(coord.x * CHUNK_SIZE, 0, coord.z * CHUNK_SIZE);
    }
    
    float getHeightAtPosition(const glm::vec2& localPos) const;
    glm::vec3 getNormalAtPosition(const glm::vec2& localPos) const;
};

class WorldStreamingSystem : public System {
public:
    struct StreamingSettings {
        float loadDistance = 256.0f;      // Distance to start loading
        float unloadDistance = 384.0f;    // Distance to unload
        float lod0Distance = 64.0f;       // Full detail distance
        float lod1Distance = 128.0f;
        float lod2Distance = 256.0f;
        // LOD3 for everything else
        
        int maxConcurrentLoads = 4;
        int maxLoadedChunks = 64;
        bool asyncLoading = true;
        bool generateCollision = true;
    } settings;
    
    WorldStreamingSystem();
    ~WorldStreamingSystem();
    
    void initialize() override;
    void update(float deltaTime) override;
    
    // Chunk management
    void loadChunk(const WorldChunk::ChunkCoord& coord);
    void unloadChunk(const WorldChunk::ChunkCoord& coord);
    void updateChunkLOD(WorldChunk* chunk, float distance);
    
    // Terrain queries
    float getHeightAtWorldPosition(const glm::vec3& worldPos);
    glm::vec3 getNormalAtWorldPosition(const glm::vec3& worldPos);
    
    // Generation
    void generateTerrain(WorldChunk* chunk);
    void generateVegetation(WorldChunk* chunk);
    void placeStaticObjects(WorldChunk* chunk);
    
private:
    std::unordered_map<WorldChunk::ChunkCoord, 
                      std::unique_ptr<WorldChunk>, 
                      WorldChunk::ChunkCoordHash> m_chunks;
    
    // Loading queue
    std::queue<WorldChunk::ChunkCoord> m_loadQueue;
    std::mutex m_loadQueueMutex;
    
    // Worker threads
    std::vector<std::thread> m_loaderThreads;
    std::atomic<bool> m_shouldExit{false};
    
    // Current center (usually player position)
    glm::vec3 m_streamingCenter;
    
    // Terrain generation
    class TerrainGenerator* m_terrainGen;
    class VegetationPlacer* m_vegetationPlacer;
    
    // Helper methods
    std::vector<WorldChunk::ChunkCoord> getChunksInRadius(
        const glm::vec3& center, float radius);
    void processLoadQueue();
    void createChunkMesh(WorldChunk* chunk);
    void uploadChunkToGPU(WorldChunk* chunk);
};

} // namespace VulkanMon
```

### Terrain Generation

```cpp
// include/world/TerrainGenerator.h
#pragma once
#include "world/WorldGrid.h"
#include <FastNoiseLite.h>

namespace VulkanMon {

class TerrainGenerator {
public:
    struct BiomeType {
        enum Type {
            Plains, Forest, Mountain, Desert, Snow, Swamp, Beach
        } type = Plains;
        
        float temperature = 0.5f;  // 0 = cold, 1 = hot
        float humidity = 0.5f;     // 0 = dry, 1 = wet
        float altitude = 0.5f;     // 0 = sea level, 1 = mountain peak
        
        // Terrain textures for this biome
        std::string textures[4];
        
        // Vegetation density
        float grassDensity = 0.5f;
        float treeDensity = 0.2f;
        float rockDensity = 0.1f;
    };
    
    TerrainGenerator(uint32_t seed = 12345);
    
    void generateHeightmap(WorldChunk* chunk);
    BiomeType getBiomeAt(const glm::vec2& worldPos);
    void generateSplatMap(WorldChunk* chunk);
    
private:
    uint32_t m_seed;
    
    // Noise generators
    FastNoiseLite m_continentNoise;   // Large scale features
    FastNoiseLite m_mountainNoise;    // Mountain ranges
    FastNoiseLite m_hillNoise;        // Rolling hills
    FastNoiseLite m_detailNoise;      // Small details
    
    // Biome noise
    FastNoiseLite m_temperatureNoise;
    FastNoiseLite m_humidityNoise;
    
    float sampleHeight(const glm::vec2& worldPos);
    float sampleBiomeBlend(const glm::vec2& worldPos, BiomeType::Type biome);
};

// Implementation
TerrainGenerator::TerrainGenerator(uint32_t seed) : m_seed(seed) {
    // Setup continent noise (large features)
    m_continentNoise.SetSeed(seed);
    m_continentNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_continentNoise.SetFrequency(0.0005f);
    m_continentNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_continentNoise.SetFractalOctaves(4);
    
    // Mountain noise
    m_mountainNoise.SetSeed(seed + 1);
    m_mountainNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_mountainNoise.SetFrequency(0.002f);
    m_mountainNoise.SetFractalType(FastNoiseLite::FractalType_Ridged);
    m_mountainNoise.SetFractalOctaves(3);
    
    // Hill noise
    m_hillNoise.SetSeed(seed + 2);
    m_hillNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_hillNoise.SetFrequency(0.01f);
    m_hillNoise.SetFractalOctaves(2);
    
    // Detail noise
    m_detailNoise.SetSeed(seed + 3);
    m_detailNoise.SetNoiseType(FastNoiseLite::NoiseType_Simplex);
    m_detailNoise.SetFrequency(0.1f);
    
    // Biome noise
    m_temperatureNoise.SetSeed(seed + 10);
    m_temperatureNoise.SetFrequency(0.001f);
    m_humidityNoise.SetSeed(seed + 11);
    m_humidityNoise.SetFrequency(0.001f);
}

void TerrainGenerator::generateHeightmap(WorldChunk* chunk) {
    const int size = WorldChunk::VERTICES_PER_SIDE;
    chunk->heightmap.resize(size * size);
    chunk->vertices.resize(size * size);
    chunk->normals.resize(size * size);
    chunk->texCoords.resize(size * size);
    
    glm::vec3 chunkWorldPos = chunk->getWorldPosition();
    
    // Generate heightmap
    for (int z = 0; z < size; ++z) {
        for (int x = 0; x < size; ++x) {
            glm::vec2 worldPos = glm::vec2(
                chunkWorldPos.x + x * (WorldChunk::CHUNK_SIZE / float(size - 1)),
                chunkWorldPos.z + z * (WorldChunk::CHUNK_SIZE / float(size - 1))
            );
            
            // Sample height with multiple octaves
            float height = sampleHeight(worldPos);
            
            int index = z * size + x;
            chunk->heightmap[index] = height;
            
            // Create vertex
            chunk->vertices[index] = glm::vec3(
                x * (WorldChunk::CHUNK_SIZE / float(size - 1)),
                height,
                z * (WorldChunk::CHUNK_SIZE / float(size - 1))
            );
            
            // Texture coordinates
            chunk->texCoords[index] = glm::vec2(
                x / float(size - 1),
                z / float(size - 1)
            );
        }
    }
    
    // Calculate normals
    for (int z = 0; z < size; ++z) {
        for (int x = 0; x < size; ++x) {
            int index = z * size + x;
            
            // Get neighboring heights
            float hL = x > 0 ? chunk->heightmap[index - 1] : chunk->heightmap[index];
            float hR = x < size - 1 ? chunk->heightmap[index + 1] : chunk->heightmap[index];
            float hD = z > 0 ? chunk->heightmap[index - size] : chunk->heightmap[index];
            float hU = z < size - 1 ? chunk->heightmap[index + size] : chunk->heightmap[index];
            
            // Calculate normal
            glm::vec3 normal(
                hL - hR,
                2.0f,
                hD - hU
            );
            chunk->normals[index] = glm::normalize(normal);
        }
    }
    
    // Generate indices for each LOD
    generateLODIndices(chunk);
}

float TerrainGenerator::sampleHeight(const glm::vec2& worldPos) {
    // Continent scale (0-100m variation)
    float continent = m_continentNoise.GetNoise(worldPos.x, worldPos.y);
    continent = (continent + 1.0f) * 0.5f * 100.0f;
    
    // Mountain scale (0-500m additional)
    float mountain = m_mountainNoise.GetNoise(worldPos.x, worldPos.y);
    mountain = glm::max(0.0f, mountain);  // Only positive values
    mountain = mountain * mountain * 500.0f;  // Square for sharp peaks
    
    // Hills (0-50m)
    float hills = m_hillNoise.GetNoise(worldPos.x, worldPos.y);
    hills = (hills + 1.0f) * 0.5f * 50.0f;
    
    // Detail (0-5m)
    float detail = m_detailNoise.GetNoise(worldPos.x, worldPos.y);
    detail = detail * 5.0f;
    
    // Combine with biome-based weights
    BiomeType biome = getBiomeAt(worldPos);
    float height = continent;
    
    switch (biome.type) {
        case BiomeType::Mountain:
            height += mountain + hills * 0.5f;
            break;
        case BiomeType::Plains:
            height += hills * 0.3f;
            break;
        case BiomeType::Forest:
            height += hills * 0.7f;
            break;
        case BiomeType::Desert:
            height += hills * 0.2f + detail;
            break;
        default:
            height += hills * 0.5f;
    }
    
    height += detail;
    
    // Water level clamp (sea level at 50m)
    if (height < 50.0f) {
        height = 50.0f - (50.0f - height) * 0.1f;  // Shallow water
    }
    
    return height;
}

} // namespace VulkanMon
```

### Vegetation System

```cpp
// include/world/VegetationSystem.h
#pragma once
#include "world/WorldGrid.h"

namespace VulkanMon {

class VegetationSystem : public System {
public:
    struct VegetationType {
        std::string modelPath;
        std::string texturePath;
        
        float minScale = 0.8f;
        float maxScale = 1.2f;
        float swayAmount = 0.1f;  // Wind sway
        float density = 1.0f;     // Per square meter
        
        bool castshadow = false;
        bool isGrass = true;      // Use instanced grass rendering
        bool reactsToPlayer = true;
    };
    
    void initialize() override;
    void update(float deltaTime) override;
    
    void generateVegetation(WorldChunk* chunk);
    void renderGrass(const std::vector<WorldChunk*>& visibleChunks);
    
private:
    std::vector<VegetationType> m_vegetationTypes;
    
    // Grass rendering (instanced)
    struct GrassInstance {
        glm::vec3 position;
        float rotation;
        float scale;
        float swayPhase;  // For wind animation
    };
    
    VkBuffer m_grassInstanceBuffer;
    VkDeviceMemory m_grassInstanceMemory;
    
    // Grass interaction
    void updateGrassInteraction(const glm::vec3& playerPos, float radius);
    
    // LOD for vegetation
    enum VegetationLOD {
        Full,      // All grass blades
        Reduced,   // 50% grass
        Billboard, // Just billboards
        None       // Too far, don't render
    };
    
    VegetationLOD getVegetationLOD(float distance);
};

void VegetationSystem::generateVegetation(WorldChunk* chunk) {
    auto* terrain = getTerrainGenerator();
    BiomeType biome = terrain->getBiomeAt(
        glm::vec2(chunk->getWorldPosition().x, chunk->getWorldPosition().z)
    );
    
    // Clear existing vegetation
    chunk->vegetation.clear();
    
    // Grass placement
    if (biome.grassDensity > 0) {
        placeGrass(chunk, biome.grassDensity);
    }
    
    // Tree placement
    if (biome.treeDensity > 0) {
        placeTrees(chunk, biome.treeDensity);
    }
    
    // Rocks and details
    if (biome.rockDensity > 0) {
        placeRocks(chunk, biome.rockDensity);
    }
    
    // Special vegetation for Pokemon habitats
    placeHabitatVegetation(chunk, biome);
}

void VegetationSystem::placeGrass(WorldChunk* chunk, float density) {
    std::mt19937 rng(chunk->coord.x * 73856093 ^ chunk->coord.z * 19349663);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    const float spacing = 1.0f / sqrtf(density);
    const int gridSize = static_cast<int>(WorldChunk::CHUNK_SIZE / spacing);
    
    for (int z = 0; z < gridSize; ++z) {
        for (int x = 0; x < gridSize; ++x) {
            // Add jitter to avoid grid pattern
            float jitterX = (dist(rng) - 0.5f) * spacing;
            float jitterZ = (dist(rng) - 0.5f) * spacing;
            
            glm::vec2 localPos(
                x * spacing + jitterX,
                z * spacing + jitterZ
            );
            
            // Check if position is valid
            if (localPos.x < 0 || localPos.x >= WorldChunk::CHUNK_SIZE ||
                localPos.y < 0 || localPos.y >= WorldChunk::CHUNK_SIZE) {
                continue;
            }
            
            float height = chunk->getHeightAtPosition(localPos);
            glm::vec3 normal = chunk->getNormalAtPosition(localPos);
            
            // Don't place grass on steep slopes
            if (normal.y < 0.7f) continue;
            
            // Don't place grass in water
            if (height < 52.0f) continue;
            
            // Random grass type
            uint8_t grassType = dist(rng) < 0.7f ? 0 : 1;  // 70% normal, 30% flowers
            
            WorldChunk::VegetationInstance grass;
            grass.position = glm::vec3(localPos.x, height, localPos.y);
            grass.rotation = dist(rng) * TWO_PI;
            grass.scale = 0.8f + dist(rng) * 0.4f;
            grass.type = grassType;
            
            chunk->vegetation.push_back(grass);
        }
    }
}

} // namespace VulkanMon
```

### Chunk Loading System

```cpp
// src/world/WorldStreamingSystem.cpp
#include "world/WorldStreamingSystem.h"
#include <execution>

namespace VulkanMon {

void WorldStreamingSystem::update(float deltaTime) {
    // Get player position
    auto playerEntity = getPlayerEntity();
    auto* transform = getComponent<Transform>(playerEntity);
    if (!transform) return;
    
    m_streamingCenter = transform->position;
    
    // Determine which chunks should be loaded
    auto requiredChunks = getChunksInRadius(m_streamingCenter, settings.loadDistance);
    
    // Schedule loading for missing chunks
    for (const auto& coord : requiredChunks) {
        auto it = m_chunks.find(coord);
        if (it == m_chunks.end() || it->second->state == WorldChunk::Unloaded) {
            scheduleChunkLoad(coord);
        }
    }
    
    // Update LODs and unload distant chunks
    std::vector<WorldChunk::ChunkCoord> toUnload;
    
    for (auto& [coord, chunk] : m_chunks) {
        glm::vec3 chunkCenter = chunk->getWorldPosition() + 
            glm::vec3(WorldChunk::CHUNK_SIZE * 0.5f, 0, WorldChunk::CHUNK_SIZE * 0.5f);
        float distance = glm::distance(m_streamingCenter, chunkCenter);
        
        if (distance > settings.unloadDistance) {
            toUnload.push_back(coord);
        } else if (chunk->state == WorldChunk::Loaded) {
            // Update LOD
            updateChunkLOD(chunk.get(), distance);
        }
    }
    
    // Unload distant chunks
    for (const auto& coord : toUnload) {
        unloadChunk(coord);
    }
    
    // Process completed loads
    processCompletedLoads();
}

void WorldStreamingSystem::loadChunk(const WorldChunk::ChunkCoord& coord) {
    auto chunk = std::make_unique<WorldChunk>();
    chunk->coord = coord;
    chunk->state = WorldChunk::Loading;
    
    // Generate terrain on worker thread
    if (settings.asyncLoading) {
        std::lock_guard<std::mutex> lock(m_loadQueueMutex);
        m_loadQueue.push(coord);
    } else {
        // Synchronous loading (for debugging)
        generateChunkContent(chunk.get());
        uploadChunkToGPU(chunk.get());
        chunk->state = WorldChunk::Loaded;
    }
    
    m_chunks[coord] = std::move(chunk);
}

void WorldStreamingSystem::generateChunkContent(WorldChunk* chunk) {
    // Generate terrain
    m_terrainGen->generateHeightmap(chunk);
    m_terrainGen->generateSplatMap(chunk);
    
    // Generate vegetation
    m_vegetationPlacer->generateVegetation(chunk);
    
    // Create mesh data
    createChunkMesh(chunk);
    
    // Generate collision mesh if needed
    if (settings.generateCollision) {
        createCollisionMesh(chunk);
    }
}

void WorldStreamingSystem::createChunkMesh(WorldChunk* chunk) {
    const int size = WorldChunk::VERTICES_PER_SIDE;
    
    // Generate indices for each LOD level
    for (int lod = 0; lod < WorldChunk::LOD_COUNT; ++lod) {
        chunk->indices[lod].clear();
        
        int step = 1 << lod;  // 1, 2, 4, 8
        
        for (int z = 0; z < size - step; z += step) {
            for (int x = 0; x < size - step; x += step) {
                // Create two triangles for each quad
                int topLeft = z * size + x;
                int topRight = topLeft + step;
                int bottomLeft = (z + step) * size + x;
                int bottomRight = bottomLeft + step;
                
                // First triangle
                chunk->indices[lod].push_back(topLeft);
                chunk->indices[lod].push_back(bottomLeft);
                chunk->indices[lod].push_back(topRight);
                
                // Second triangle
                chunk->indices[lod].push_back(topRight);
                chunk->indices[lod].push_back(bottomLeft);
                chunk->indices[lod].push_back(bottomRight);
            }
        }
    }
}

void WorldStreamingSystem::processLoadQueue() {
    while (!m_shouldExit) {
        WorldChunk::ChunkCoord coord;
        
        {
            std::unique_lock<std::mutex> lock(m_loadQueueMutex);
            m_loadCondition.wait(lock, [this] { 
                return !m_loadQueue.empty() || m_shouldExit; 
            });
            
            if (m_shouldExit) break;
            
            coord = m_loadQueue.front();
            m_loadQueue.pop();
        }
        
        // Find the chunk
        auto it = m_chunks.find(coord);
        if (it != m_chunks.end()) {
            generateChunkContent(it->second.get());
            
            // Mark as ready for GPU upload (must be done on main thread)
            it->second->state = WorldChunk::Loading;
            m_completedLoads.push(coord);
        }
    }
}

void WorldStreamingSystem::updateChunkLOD(WorldChunk* chunk, float distance) {
    WorldChunk::LODLevel newLOD;
    
    if (distance < settings.lod0Distance) {
        newLOD = WorldChunk::LOD0;
    } else if (distance < settings.lod1Distance) {
        newLOD = WorldChunk::LOD1;
    } else if (distance < settings.lod2Distance) {
        newLOD = WorldChunk::LOD2;
    } else {
        newLOD = WorldChunk::LOD3;
    }
    
    if (chunk->currentLOD != newLOD) {
        chunk->currentLOD = newLOD;
        // Update render component if needed
        updateChunkRendering(chunk);
    }
}

} // namespace VulkanMon
```

### Seamless Chunk Transitions

```cpp
// include/world/ChunkBorderBlending.h
#pragma once

namespace VulkanMon {

class ChunkBorderBlending {
public:
    // Blend heightmaps at chunk borders to prevent seams
    static void blendChunkBorders(WorldChunk* chunkA, WorldChunk* chunkB) {
        // Determine shared edge
        Edge sharedEdge = getSharedEdge(chunkA->coord, chunkB->coord);
        if (sharedEdge == Edge::None) return;
        
        const int size = WorldChunk::VERTICES_PER_SIDE;
        
        // Blend heights along the edge
        for (int i = 0; i < size; ++i) {
            int indexA = getEdgeIndex(chunkA, sharedEdge, i);
            int indexB = getEdgeIndex(chunkB, getOppositeEdge(sharedEdge), i);
            
            // Average the heights
            float avgHeight = (chunkA->heightmap[indexA] + 
                              chunkB->heightmap[indexB]) * 0.5f;
            
            chunkA->heightmap[indexA] = avgHeight;
            chunkB->heightmap[indexB] = avgHeight;
            
            // Update vertices
            chunkA->vertices[indexA].y = avgHeight;
            chunkB->vertices[indexB].y = avgHeight;
        }
        
        // Recalculate normals for the edge vertices
        recalculateEdgeNormals(chunkA, sharedEdge);
        recalculateEdgeNormals(chunkB, getOppositeEdge(sharedEdge));
    }
    
    // Create transition meshes for LOD boundaries
    static void createLODTransition(WorldChunk* highLOD, WorldChunk* lowLOD) {
        // Generate special indices that connect different LOD levels
        // This prevents cracks between chunks with different detail levels
    }
};

} // namespace VulkanMon
```

### Grass Interaction System

```cpp
// include/world/GrassInteraction.h
#pragma once

namespace VulkanMon {

class GrassInteractionSystem : public System {
public:
    struct GrassBlade {
        glm::vec3 basePosition;
        glm::vec3 tipOffset;     // Current bend
        glm::vec3 targetOffset;  // Target bend
        float restoreSpeed = 2.0f;
        float bendStrength = 0.0f;
    };
    
    void update(float deltaTime) override {
        // Get all entities that can affect grass
        auto movers = getEntitiesWithComponents<Transform, PhysicsComponent>();
        
        for (auto& [chunkCoord, chunk] : getLoadedChunks()) {
            updateChunkGrass(chunk.get(), movers, deltaTime);
        }
    }
    
private:
    void updateChunkGrass(WorldChunk* chunk, 
                         const std::vector<EntityId>& movers,
                         float deltaTime) {
        
        for (auto& grass : chunk->grassBlades) {
            // Check for nearby movers
            float influence = 0.0f;
            glm::vec3 bendDirection(0.0f);
            
            for (EntityId mover : movers) {
                auto* transform = getComponent<Transform>(mover);
                float distance = glm::distance(transform->position, 
                                             grass.basePosition);
                
                if (distance < 2.0f) {  // Influence radius
                    float strength = 1.0f - (distance / 2.0f);
                    glm::vec3 away = grass.basePosition - transform->position;
                    away.y = 0;  // Only bend horizontally
                    away = glm::normalize(away);
                    
                    bendDirection += away * strength;
                    influence = glm::max(influence, strength);
                }
            }
            
            // Apply wind
            float windTime = getCurrentTime() * 2.0f;
            glm::vec3 windBend(
                sin(windTime + grass.basePosition.x * 0.1f) * 0.1f,
                0,
                cos(windTime * 0.7f + grass.basePosition.z * 0.1f) * 0.1f
            );
            
            // Combine influences
            grass.targetOffset = bendDirection * influence + windBend;
            
            // Smooth interpolation
            grass.tipOffset = glm::mix(
                grass.tipOffset,
                grass.targetOffset,
                1.0f - exp(-grass.restoreSpeed * deltaTime)
            );
        }
        
        // Update GPU buffer
        updateGrassInstanceBuffer(chunk);
    }
};

} // namespace VulkanMon
```

### Dynamic Environment System

```cpp
// include/world/DynamicEnvironment.h
#pragma once

namespace VulkanMon {

class DynamicEnvironmentSystem : public System {
public:
    struct TimeOfDay {
        float hour = 12.0f;  // 0-24
        
        glm::vec3 getSunDirection() const {
            float angle = (hour - 6.0f) / 12.0f * M_PI;  // 6am to 6pm
            return glm::vec3(
                cos(angle),
                sin(angle),
                0.0f
            );
        }
        
        glm::vec3 getSkyColor() const {
            // Dawn/dusk colors
            if (hour < 6 || hour > 18) {
                return glm::vec3(0.1f, 0.1f, 0.2f);  // Night
            } else if (hour < 8) {
                float t = (hour - 6) / 2.0f;
                return glm::mix(
                    glm::vec3(0.1f, 0.1f, 0.2f),  // Night
                    glm::vec3(1.0f, 0.6f, 0.3f),  // Dawn
                    t
                );
            } else if (hour > 16) {
                float t = (hour - 16) / 2.0f;
                return glm::mix(
                    glm::vec3(0.5f, 0.7f, 1.0f),  // Day
                    glm::vec3(1.0f, 0.4f, 0.2f),  // Dusk
                    t
                );
            } else {
                return glm::vec3(0.5f, 0.7f, 1.0f);  // Day
            }
        }
    };
    
    struct Weather {
        enum Type {
            Clear, Cloudy, Rain, Storm, Snow, Fog
        } type = Clear;
        
        float intensity = 0.0f;
        float transitionSpeed = 0.1f;
    };
    
    TimeOfDay timeOfDay;
    Weather weather;
    
    void update(float deltaTime) override {
        // Update time
        timeOfDay.hour += deltaTime * 0.1f;  // 10x speed
        if (timeOfDay.hour >= 24.0f) {
            timeOfDay.hour -= 24.0f;
        }
        
        // Update lighting
        updateSceneLighting();
        
        // Update weather
        updateWeatherEffects(deltaTime);
        
        // Trigger time-based events
        checkTimeEvents();
    }
    
private:
    void updateSceneLighting() {
        auto& lighting = getLightingSystem();
        
        // Sun light
        lighting.setSunDirection(timeOfDay.getSunDirection());
        lighting.setSunColor(timeOfDay.getSkyColor());
        
        // Ambient light changes with time
        float ambientStrength = 0.1f;
        if (timeOfDay.hour > 6 && timeOfDay.hour < 18) {
            ambientStrength = 0.3f;
        }
        lighting.setAmbientLight(timeOfDay.getSkyColor() * ambientStrength);
        
        // Fog for morning/weather
        if (timeOfDay.hour < 8 || weather.type == Weather::Fog) {
            lighting.setFogDensity(0.02f);
        } else {
            lighting.setFogDensity(0.005f);
        }
    }
    
    void checkTimeEvents() {
        // Spawn nocturnal Pokemon at night
        if (timeOfDay.hour == 20.0f) {
            spawnNocturnalCreatures();
        }
        
        // Morning Pokemon wake up
        if (timeOfDay.hour == 6.0f) {
            wakeDiurnalCreatures();
        }
    }
};

} // namespace VulkanMon
```

## Performance Optimization

### Chunk Memory Pool

```cpp
class ChunkMemoryPool {
    struct ChunkMemory {
        VkBuffer vertexBuffer;
        VkBuffer indexBuffer;
        VkDeviceMemory memory;
        bool inUse = false;
    };
    
    std::vector<ChunkMemory> m_pool;
    std::mutex m_poolMutex;
    
public:
    ChunkMemory* acquire() {
        std::lock_guard<std::mutex> lock(m_poolMutex);
        
        for (auto& mem : m_pool) {
            if (!mem.inUse) {
                mem.inUse = true;
                return &mem;
            }
        }
        
        // Allocate new if pool exhausted
        m_pool.emplace_back(allocateChunkMemory());
        m_pool.back().inUse = true;
        return &m_pool.back();
    }
    
    void release(ChunkMemory* mem) {
        std::lock_guard<std::mutex> lock(m_poolMutex);
        mem->inUse = false;
    }
};
```

### Frustum Culling

```cpp
class FrustumCuller {
public:
    struct Frustum {
        glm::vec4 planes[6];  // Left, Right, Top, Bottom, Near, Far
    };
    
    Frustum extractFrustum(const glm::mat4& viewProjection) {
        Frustum frustum;
        
        // Extract planes from view-projection matrix
        // Left plane
        frustum.planes[0] = glm::vec4(
            viewProjection[0][3] + viewProjection[0][0],
            viewProjection[1][3] + viewProjection[1][0],
            viewProjection[2][3] + viewProjection[2][0],
            viewProjection[3][3] + viewProjection[3][0]
        );
        
        // ... extract other planes
        
        // Normalize planes
        for (int i = 0; i < 6; ++i) {
            float length = glm::length(glm::vec3(frustum.planes[i]));
            frustum.planes[i] /= length;
        }
        
        return frustum;
    }
    
    bool isChunkVisible(const Frustum& frustum, const WorldChunk* chunk) {
        // Get chunk bounding box
        glm::vec3 min = chunk->getWorldPosition();
        glm::vec3 max = min + glm::vec3(WorldChunk::CHUNK_SIZE, 200, WorldChunk::CHUNK_SIZE);
        
        // Test against all planes
        for (int i = 0; i < 6; ++i) {
            glm::vec3 positive(
                frustum.planes[i].x > 0 ? max.x : min.x,
                frustum.planes[i].y > 0 ? max.y : min.y,
                frustum.planes[i].z > 0 ? max.z : min.z
            );
            
            float distance = glm::dot(glm::vec3(frustum.planes[i]), positive) + 
                           frustum.planes[i].w;
            
            if (distance < 0) {
                return false;  // Outside frustum
            }
        }
        
        return true;
    }
};
```

## Testing Framework

```cpp
// tests_cpp/world/test_WorldStreaming.cpp
TEST_CASE("World Streaming", "[world]") {
    WorldStreamingSystem streaming;
    
    SECTION("Chunk loading radius") {
        // Set player position
        streaming.setStreamingCenter(glm::vec3(0, 0, 0));
        streaming.update(0.016f);
        
        // Check loaded chunks
        auto loadedChunks = streaming.getLoadedChunks();
        
        // Should have loaded chunks in radius
        int expectedChunks = pow(
            ceil(streaming.settings.loadDistance / WorldChunk::CHUNK_SIZE) * 2 + 1, 
            2
        );
        CHECK(loadedChunks.size() <= expectedChunks);
    }
    
    SECTION("LOD transitions") {
        auto chunk = streaming.getChunk({0, 0});
        
        streaming.updateChunkLOD(chunk, 30.0f);
        CHECK(chunk->currentLOD == WorldChunk::LOD0);
        
        streaming.updateChunkLOD(chunk, 100.0f);
        CHECK(chunk->currentLOD == WorldChunk::LOD1);
        
        streaming.updateChunkLOD(chunk, 300.0f);
        CHECK(chunk->currentLOD == WorldChunk::LOD3);
    }
    
    SECTION("Height queries") {
        streaming.loadChunk({0, 0});
        
        float height = streaming.getHeightAtWorldPosition(glm::vec3(32, 0, 32));
        CHECK(height > 0);
        
        glm::vec3 normal = streaming.getNormalAtWorldPosition(glm::vec3(32, 0, 32));
        CHECK(glm::length(normal) == Approx(1.0f));
    }
}
```

## Performance Benchmarks

```cpp
static void BM_ChunkGeneration(benchmark::State& state) {
    TerrainGenerator generator;
    
    for (auto _ : state) {
        WorldChunk chunk;
        chunk.coord = {rand() % 100, rand() % 100};
        generator.generateHeightmap(&chunk);
    }
}
BENCHMARK(BM_ChunkGeneration);

static void BM_StreamingUpdate_100_Chunks(benchmark::State& state) {
    WorldStreamingSystem streaming;
    
    // Pre-load chunks
    for (int x = -5; x < 5; ++x) {
        for (int z = -5; z < 5; ++z) {
            streaming.loadChunk({x, z});
        }
    }
    
    for (auto _ : state) {
        streaming.setStreamingCenter(
            glm::vec3(rand() % 500, 0, rand() % 500)
        );
        streaming.update(0.016f);
    }
}
BENCHMARK(BM_StreamingUpdate_100_Chunks);
```

## Week 5-6 Daily Milestones

### Week 5
- **Day 29-30**: Basic chunk system and terrain generation
- **Day 31**: Height-based terrain with biomes
- **Day 32**: Chunk loading/unloading system
- **Day 33**: LOD system implementation
- **Day 34**: Vegetation placement
- **Day 35**: Grass rendering and interaction

### Week 6
- **Day 36**: Async chunk loading
- **Day 37**: Chunk border blending
- **Day 38**: Frustum culling
- **Day 39**: Dynamic time of day
- **Day 40**: Weather system
- **Day 41**: Performance optimization
- **Day 42**: Testing and polish

## Performance Targets

- **Chunk Generation**: < 50ms per chunk
- **Streaming Update**: < 2ms per frame
- **Height Query**: < 0.01ms
- **Visible Chunks**: 60+ FPS with 50 chunks
- **Memory**: < 10MB per chunk
- **LOD Switching**: Seamless, no visible pop-in