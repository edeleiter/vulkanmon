# Pokemon Model Integration Pipeline: Blender to Vulkan
## Complete Guide for Importing Blender Pokemon Models

## Overview
This guide covers the entire pipeline for getting your Pokemon `.blend` files into VulkanMon, including export settings, optimization, animation setup, and runtime loading.

## Part 1: Blender Export Pipeline

### Automated Export Script

First, create a Python script for Blender to batch export all Pokemon models with consistent settings:

```python
# tools/blender_scripts/export_pokemon.py
import bpy
import os
import json
from pathlib import Path

class PokemonExporter:
    def __init__(self, output_dir="assets/models/creatures"):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Export settings for different asset types
        self.model_settings = {
            'format': 'GLTF',  # or 'FBX' - GLTF recommended for PBR
            'lod_levels': [1.0, 0.5, 0.25, 0.125],  # LOD decimation ratios
            'texture_sizes': {
                'diffuse': 1024,
                'normal': 512,
                'metallic_roughness': 512,
                'emissive': 256
            }
        }
        
    def export_pokemon(self, blend_file):
        """Export a single Pokemon with all LODs and animations"""
        bpy.ops.wm.open_mainfile(filepath=blend_file)
        
        # Get Pokemon name from file
        pokemon_name = Path(blend_file).stem.lower()
        pokemon_dir = self.output_dir / pokemon_name
        pokemon_dir.mkdir(exist_ok=True)
        
        # Find the main mesh
        pokemon_mesh = self.find_pokemon_mesh()
        if not pokemon_mesh:
            print(f"No mesh found in {blend_file}")
            return
        
        # Export LODs
        self.export_lods(pokemon_mesh, pokemon_dir)
        
        # Export animations
        self.export_animations(pokemon_dir)
        
        # Export textures
        self.export_textures(pokemon_mesh, pokemon_dir)
        
        # Generate metadata
        self.generate_metadata(pokemon_name, pokemon_dir)
        
    def find_pokemon_mesh(self):
        """Find the main Pokemon mesh in the scene"""
        # Look for the highest poly mesh object
        meshes = [obj for obj in bpy.data.objects if obj.type == 'MESH']
        if not meshes:
            return None
            
        # Sort by vertex count and return the main model
        meshes.sort(key=lambda x: len(x.data.vertices), reverse=True)
        return meshes[0]
    
    def export_lods(self, mesh_obj, output_dir):
        """Generate and export LOD levels"""
        original_mesh = mesh_obj.data.copy()
        
        for i, ratio in enumerate(self.model_settings['lod_levels']):
            # Apply decimation for LODs > 0
            if i > 0:
                self.decimate_mesh(mesh_obj, ratio)
            
            # Export settings
            export_path = str(output_dir / f"model_lod{i}.gltf")
            
            # Select only the Pokemon mesh
            bpy.ops.object.select_all(action='DESELECT')
            mesh_obj.select_set(True)
            bpy.context.view_layer.objects.active = mesh_obj
            
            # Export as GLTF
            bpy.ops.export_scene.gltf(
                filepath=export_path,
                use_selection=True,
                export_format='GLTF_SEPARATE',  # Separate .bin and textures
                export_texcoords=True,
                export_normals=True,
                export_colors=True,
                export_materials='EXPORT',
                export_cameras=False,
                export_lights=False,
                export_animations=False,  # Handle separately
                export_apply=True,
                export_yup=False,  # Use Z-up for Vulkan
                export_tangents=True  # For normal mapping
            )
            
            # Restore original mesh for next LOD
            mesh_obj.data = original_mesh.copy()
        
        # Cleanup
        bpy.data.meshes.remove(original_mesh)
    
    def decimate_mesh(self, mesh_obj, ratio):
        """Apply decimation modifier for LOD generation"""
        # Remove existing decimation modifiers
        for mod in mesh_obj.modifiers:
            if mod.type == 'DECIMATE':
                mesh_obj.modifiers.remove(mod)
        
        # Add new decimation modifier
        decimate = mesh_obj.modifiers.new(name="Decimate", type='DECIMATE')
        decimate.ratio = ratio
        decimate.use_collapse_triangulate = True
        
        # Apply modifier
        bpy.context.view_layer.objects.active = mesh_obj
        bpy.ops.object.modifier_apply(modifier=decimate.name)
    
    def export_animations(self, output_dir):
        """Export all animations as separate files"""
        animations_dir = output_dir / "animations"
        animations_dir.mkdir(exist_ok=True)
        
        # Common Pokemon animations
        animation_names = [
            'idle', 'walk', 'run', 'attack1', 'attack2',
            'special_attack', 'hurt', 'faint', 'sleep',
            'eat', 'happy', 'roar'
        ]
        
        for action in bpy.data.actions:
            # Clean action name
            anim_name = action.name.lower().replace(' ', '_')
            
            # Check if it's a known animation type
            for known_anim in animation_names:
                if known_anim in anim_name:
                    export_path = str(animations_dir / f"{known_anim}.gltf")
                    
                    # Set active action
                    if bpy.context.object:
                        bpy.context.object.animation_data.action = action
                    
                    # Export with animation
                    bpy.ops.export_scene.gltf(
                        filepath=export_path,
                        use_selection=True,
                        export_format='GLTF_SEPARATE',
                        export_animations=True,
                        export_frame_range=True,
                        export_force_sampling=True,
                        export_nla_strips=False,
                        export_bake_animation=True,
                        export_optimize_animation_size=True
                    )
                    break
    
    def export_textures(self, mesh_obj, output_dir):
        """Export and optimize textures"""
        textures_dir = output_dir / "textures"
        textures_dir.mkdir(exist_ok=True)
        
        # Process materials
        for mat_slot in mesh_obj.material_slots:
            if not mat_slot.material:
                continue
                
            mat = mat_slot.material
            
            # Export different texture types
            self.export_texture_map(mat, 'Base Color', textures_dir / 'diffuse.png', 
                                   self.model_settings['texture_sizes']['diffuse'])
            self.export_texture_map(mat, 'Normal', textures_dir / 'normal.png',
                                   self.model_settings['texture_sizes']['normal'])
            self.export_texture_map(mat, 'Metallic', textures_dir / 'metallic.png',
                                   self.model_settings['texture_sizes']['metallic_roughness'])
            self.export_texture_map(mat, 'Roughness', textures_dir / 'roughness.png',
                                   self.model_settings['texture_sizes']['metallic_roughness'])
            self.export_texture_map(mat, 'Emission', textures_dir / 'emissive.png',
                                   self.model_settings['texture_sizes']['emissive'])
            
            # Generate shiny variant
            self.generate_shiny_texture(textures_dir)
    
    def generate_shiny_texture(self, textures_dir):
        """Generate shiny Pokemon texture variant"""
        diffuse_path = textures_dir / 'diffuse.png'
        if not diffuse_path.exists():
            return
            
        # Load and modify for shiny variant
        import numpy as np
        from PIL import Image
        
        img = Image.open(diffuse_path)
        img_array = np.array(img).astype(float) / 255.0
        
        # Hue shift for shiny (example: shift towards purple/gold)
        # This is simplified - you might want more sophisticated color grading
        img_array[:, :, 0] *= 0.8  # Reduce red
        img_array[:, :, 1] *= 0.9  # Reduce green  
        img_array[:, :, 2] *= 1.2  # Increase blue
        
        # Add slight metallic sheen
        img_array = np.clip(img_array * 1.1, 0, 1)
        
        # Save shiny variant
        shiny_img = Image.fromarray((img_array * 255).astype(np.uint8))
        shiny_img.save(textures_dir / 'diffuse_shiny.png')
    
    def generate_metadata(self, pokemon_name, output_dir):
        """Generate metadata JSON for the Pokemon"""
        metadata = {
            'name': pokemon_name,
            'models': {
                'lod0': 'model_lod0.gltf',
                'lod1': 'model_lod1.gltf',
                'lod2': 'model_lod2.gltf',
                'lod3': 'model_lod3.gltf'
            },
            'textures': {
                'diffuse': 'textures/diffuse.png',
                'normal': 'textures/normal.png',
                'metallic': 'textures/metallic.png',
                'roughness': 'textures/roughness.png',
                'emissive': 'textures/emissive.png',
                'diffuse_shiny': 'textures/diffuse_shiny.png'
            },
            'animations': {
                'idle': 'animations/idle.gltf',
                'walk': 'animations/walk.gltf',
                'run': 'animations/run.gltf',
                'attack': 'animations/attack1.gltf',
                'hurt': 'animations/hurt.gltf',
                'faint': 'animations/faint.gltf'
            },
            'bounds': self.calculate_bounds(mesh_obj),
            'polycount': {
                'lod0': len(mesh_obj.data.vertices),
                'lod1': int(len(mesh_obj.data.vertices) * 0.5),
                'lod2': int(len(mesh_obj.data.vertices) * 0.25),
                'lod3': int(len(mesh_obj.data.vertices) * 0.125)
            }
        }
        
        with open(output_dir / 'metadata.json', 'w') as f:
            json.dump(metadata, f, indent=2)
    
    def calculate_bounds(self, mesh_obj):
        """Calculate bounding box and sphere"""
        vertices = mesh_obj.data.vertices
        
        min_coord = [float('inf')] * 3
        max_coord = [float('-inf')] * 3
        
        for vert in vertices:
            for i in range(3):
                min_coord[i] = min(min_coord[i], vert.co[i])
                max_coord[i] = max(max_coord[i], vert.co[i])
        
        center = [(min_coord[i] + max_coord[i]) / 2 for i in range(3)]
        size = [max_coord[i] - min_coord[i] for i in range(3)]
        radius = max(size) / 2
        
        return {
            'center': center,
            'size': size,
            'radius': radius
        }

# Batch export script
def batch_export_pokemon(blend_files_dir, output_dir):
    """Export all Pokemon from a directory of .blend files"""
    exporter = PokemonExporter(output_dir)
    blend_files = Path(blend_files_dir).glob("*.blend")
    
    for blend_file in blend_files:
        print(f"Exporting {blend_file.name}...")
        try:
            exporter.export_pokemon(str(blend_file))
            print(f"✓ Successfully exported {blend_file.stem}")
        except Exception as e:
            print(f"✗ Failed to export {blend_file.stem}: {e}")

# Run from Blender: 
# blender --background --python export_pokemon.py -- /path/to/blends /path/to/output
if __name__ == "__main__":
    import sys
    if "--" in sys.argv:
        argv = sys.argv[sys.argv.index("--") + 1:]
        if len(argv) >= 2:
            batch_export_pokemon(argv[0], argv[1])
```

## Part 2: Runtime Model Loader

### GLTF Loader Implementation

```cpp
// include/models/PokemonModelLoader.h
#pragma once
#include <tiny_gltf.h>
#include "rendering/Model.h"
#include "creature/CreatureData.h"

namespace VulkanMon {

class PokemonModelLoader {
public:
    struct PokemonModel {
        // Model data for each LOD
        struct LODModel {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;
            VkBuffer vertexBuffer;
            VkBuffer indexBuffer;
            VkDeviceMemory vertexMemory;
            VkDeviceMemory indexMemory;
            uint32_t indexCount;
        };
        std::array<LODModel, 4> lods;
        
        // Textures
        struct TextureSet {
            VkImage diffuse;
            VkImage normal;
            VkImage metallicRoughness;
            VkImage emissive;
            VkImageView diffuseView;
            VkImageView normalView;
            VkImageView metallicRoughnessView;
            VkImageView emissiveView;
            VkSampler sampler;
        };
        TextureSet normalTextures;
        TextureSet shinyTextures;
        
        // Animations
        struct Animation {
            std::string name;
            float duration;
            std::vector<AnimationChannel> channels;
        };
        std::unordered_map<std::string, Animation> animations;
        
        // Metadata
        std::string name;
        glm::vec3 boundingCenter;
        glm::vec3 boundingSize;
        float boundingRadius;
        
        // Skeletal data
        std::vector<Joint> skeleton;
        std::vector<glm::mat4> inverseBindMatrices;
    };
    
    PokemonModelLoader(VkDevice device, VkPhysicalDevice physicalDevice,
                      VkCommandPool commandPool, VkQueue queue);
    
    // Load a Pokemon model with all its assets
    std::shared_ptr<PokemonModel> loadPokemon(const std::string& pokemonName);
    
    // Load specific LOD
    void loadLOD(PokemonModel* model, int lodLevel, const std::string& gltfPath);
    
    // Animation loading
    void loadAnimation(PokemonModel* model, const std::string& animName, 
                      const std::string& gltfPath);
    
private:
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;
    VkCommandPool m_commandPool;
    VkQueue m_queue;
    
    // Cache loaded models
    std::unordered_map<std::string, std::shared_ptr<PokemonModel>> m_modelCache;
    
    // GLTF loading helpers
    void loadGLTFModel(const std::string& path, PokemonModel::LODModel& lod);
    void processNode(const tinygltf::Model& model, const tinygltf::Node& node,
                     glm::mat4 parentTransform, PokemonModel::LODModel& lod);
    void processMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh,
                    glm::mat4 transform, PokemonModel::LODModel& lod);
    
    // Texture loading
    VkImage loadTexture(const std::string& path, VkFormat format);
    void createTextureImageView(VkImage image, VkImageView& imageView, VkFormat format);
    void createTextureSampler(VkSampler& sampler);
    
    // Buffer creation
    void createVertexBuffer(PokemonModel::LODModel& lod);
    void createIndexBuffer(PokemonModel::LODModel& lod);
};

// Implementation snippet
std::shared_ptr<PokemonModel> PokemonModelLoader::loadPokemon(const std::string& pokemonName) {
    // Check cache
    auto it = m_modelCache.find(pokemonName);
    if (it != m_modelCache.end()) {
        return it->second;
    }
    
    auto model = std::make_shared<PokemonModel>();
    model->name = pokemonName;
    
    // Load metadata
    std::string basePath = "assets/models/creatures/" + pokemonName + "/";
    std::ifstream metadataFile(basePath + "metadata.json");
    nlohmann::json metadata;
    metadataFile >> metadata;
    
    // Load each LOD
    for (int i = 0; i < 4; ++i) {
        std::string lodPath = basePath + metadata["models"]["lod" + std::to_string(i)];
        loadLOD(model.get(), i, lodPath);
    }
    
    // Load textures
    model->normalTextures.diffuse = loadTexture(
        basePath + metadata["textures"]["diffuse"], 
        VK_FORMAT_R8G8B8A8_SRGB
    );
    model->normalTextures.normal = loadTexture(
        basePath + metadata["textures"]["normal"],
        VK_FORMAT_R8G8B8A8_UNORM
    );
    
    // Load shiny variant
    model->shinyTextures.diffuse = loadTexture(
        basePath + metadata["textures"]["diffuse_shiny"],
        VK_FORMAT_R8G8B8A8_SRGB
    );
    
    // Load animations
    for (auto& [key, value] : metadata["animations"].items()) {
        loadAnimation(model.get(), key, basePath + value);
    }
    
    // Parse bounds
    model->boundingRadius = metadata["bounds"]["radius"];
    for (int i = 0; i < 3; ++i) {
        model->boundingCenter[i] = metadata["bounds"]["center"][i];
        model->boundingSize[i] = metadata["bounds"]["size"][i];
    }
    
    // Cache the loaded model
    m_modelCache[pokemonName] = model;
    
    return model;
}

} // namespace VulkanMon
```

## Part 3: Pokemon Rendering Component

```cpp
// include/rendering/PokemonRenderer.h
#pragma once

namespace VulkanMon {

class PokemonRenderComponent : public Component {
public:
    std::string speciesName;
    std::shared_ptr<PokemonModel> model;
    
    // Current state
    int currentLOD = 0;
    bool isShiny = false;
    std::string currentAnimation = "idle";
    float animationTime = 0.0f;
    float animationSpeed = 1.0f;
    
    // Animation blending
    struct AnimationBlend {
        std::string fromAnimation;
        std::string toAnimation;
        float blendFactor = 0.0f;
        float blendDuration = 0.3f;
    } blend;
    
    // Customization
    glm::vec3 colorTint = glm::vec3(1.0f);  // For regional variants
    float scale = 1.0f;  // Size variations
    
    // Performance
    float lastLODUpdateDistance = 0.0f;
    bool frustumCulled = false;
};

class PokemonRenderSystem : public System {
public:
    void update(float deltaTime) override {
        auto& camera = getActiveCamera();
        auto frustum = camera.getFrustum();
        
        for (auto& [entity, renderComp] : getComponents<PokemonRenderComponent>()) {
            auto* transform = getComponent<Transform>(entity);
            if (!transform) continue;
            
            // Update LOD based on distance
            float distance = glm::distance(camera.position, transform->position);
            updateLOD(renderComp, distance);
            
            // Frustum culling
            renderComp->frustumCulled = !isInFrustum(
                frustum, 
                transform->position, 
                renderComp->model->boundingRadius * transform->scale.x
            );
            
            if (renderComp->frustumCulled) continue;
            
            // Update animation
            updateAnimation(renderComp, deltaTime);
            
            // Queue for rendering
            queuePokemonDraw(entity, renderComp, transform);
        }
    }
    
private:
    void updateLOD(PokemonRenderComponent* comp, float distance) {
        int targetLOD = 0;
        
        if (distance > 100.0f) targetLOD = 3;
        else if (distance > 50.0f) targetLOD = 2;
        else if (distance > 25.0f) targetLOD = 1;
        
        if (comp->currentLOD != targetLOD) {
            comp->currentLOD = targetLOD;
            comp->lastLODUpdateDistance = distance;
        }
    }
    
    void updateAnimation(PokemonRenderComponent* comp, float deltaTime) {
        comp->animationTime += deltaTime * comp->animationSpeed;
        
        // Handle animation blending
        if (comp->blend.blendFactor < 1.0f && !comp->blend.toAnimation.empty()) {
            comp->blend.blendFactor += deltaTime / comp->blend.blendDuration;
            
            if (comp->blend.blendFactor >= 1.0f) {
                comp->currentAnimation = comp->blend.toAnimation;
                comp->blend.fromAnimation.clear();
                comp->blend.toAnimation.clear();
                comp->blend.blendFactor = 0.0f;
            }
        }
        
        // Loop animation
        auto& anim = comp->model->animations[comp->currentAnimation];
        if (comp->animationTime > anim.duration) {
            comp->animationTime = fmod(comp->animationTime, anim.duration);
        }
    }
    
    void queuePokemonDraw(EntityId entity, PokemonRenderComponent* comp, Transform* transform) {
        RenderCommand cmd;
        cmd.type = RenderCommand::Pokemon;
        cmd.entity = entity;
        cmd.modelMatrix = transform->getMatrix();
        cmd.lod = comp->currentLOD;
        cmd.vertexBuffer = comp->model->lods[comp->currentLOD].vertexBuffer;
        cmd.indexBuffer = comp->model->lods[comp->currentLOD].indexBuffer;
        cmd.indexCount = comp->model->lods[comp->currentLOD].indexCount;
        
        // Select texture set
        auto& textures = comp->isShiny ? 
            comp->model->shinyTextures : comp->model->normalTextures;
        
        cmd.textures = {
            textures.diffuseView,
            textures.normalView,
            textures.metallicRoughnessView,
            textures.emissiveView
        };
        
        // Animation data
        cmd.animationTime = comp->animationTime;
        cmd.boneMatrices = calculateBoneMatrices(comp);
        
        m_renderQueue.push(cmd);
    }
};

} // namespace VulkanMon
```

## Part 4: Integration with Game Systems

```cpp
// src/game/PokemonFactory.cpp
class PokemonFactory {
public:
    EntityId createPokemon(uint16_t speciesId, uint8_t level, const glm::vec3& position) {
        auto& ecs = ECS::getInstance();
        EntityId pokemon = ecs.createEntity();
        
        // Get species data
        const auto& species = m_speciesDatabase[speciesId];
        
        // Transform
        auto* transform = ecs.addComponent<Transform>(pokemon);
        transform->position = position;
        transform->scale = glm::vec3(species.height / 1.8f);  // Normalize to ~1m base
        
        // Creature component
        auto* creature = ecs.addComponent<CreatureComponent>(pokemon);
        creature->speciesId = speciesId;
        creature->level = level;
        creature->personalityValue = rand();
        creature->ivs = IVs::random();
        creature->recalculateStats();
        
        // Pokemon render component
        auto* render = ecs.addComponent<PokemonRenderComponent>(pokemon);
        render->speciesName = species.name;
        render->model = m_modelLoader->loadPokemon(toLowerCase(species.name));
        render->isShiny = creature->isShiny();
        render->scale = 0.9f + (creature->ivs.hp / 31.0f) * 0.2f;  // Size variation
        
        // Physics
        auto* physics = ecs.addComponent<PhysicsComponent>(pokemon);
        physics->bodyType = BodyType::Kinematic;
        physics->shapeType = PhysicsComponent::Capsule;
        physics->shapeSize = glm::vec3(
            render->model->boundingRadius * 0.8f,
            render->model->boundingSize.y,
            0
        );
        physics->collisionGroup = CollisionGroup::Creature;
        
        // AI
        auto* ai = ecs.addComponent<CreatureAIComponent>(pokemon);
        ai->aiType = CreatureAIComponent::Wild;
        ai->homePosition = position;
        
        // Audio
        auto* audio = ecs.addComponent<AudioComponent>(pokemon);
        audio->sounds["cry"] = "audio/cries/" + species.name + ".ogg";
        audio->sounds["footstep"] = "audio/footsteps/grass_medium.ogg";
        
        return pokemon;
    }
    
private:
    PokemonModelLoader* m_modelLoader;
    std::unordered_map<uint16_t, CreatureSpecies> m_speciesDatabase;
};
```

## Part 5: Optimization Strategies

### Texture Atlas System

```cpp
class PokemonTextureAtlas {
public:
    void buildAtlas(const std::vector<std::string>& pokemonNames) {
        // Combine textures into atlases for better performance
        struct AtlasEntry {
            std::string pokemon;
            glm::vec4 uvBounds;  // x, y, width, height in atlas
        };
        
        // Create separate atlases for different texture types
        createAtlas(pokemonNames, "diffuse", 4096);
        createAtlas(pokemonNames, "normal", 2048);
        createAtlas(pokemonNames, "metallic_roughness", 2048);
    }
    
private:
    void createAtlas(const std::vector<std::string>& names, 
                    const std::string& textureType,
                    uint32_t atlasSize) {
        // Use stb_rect_pack for efficient texture packing
    }
};
```

### Instanced Rendering for Multiple Pokemon

```cpp
class PokemonInstanceRenderer {
    struct InstanceData {
        glm::mat4 modelMatrix;
        glm::vec4 colorTint;
        float animationTime;
        uint32_t textureIndex;  // Index into texture array
    };
    
    void renderPokemonBatch(const std::vector<EntityId>& pokemonEntities) {
        // Group by species and LOD for instancing
        std::map<std::pair<std::string, int>, std::vector<InstanceData>> batches;
        
        for (EntityId entity : pokemonEntities) {
            auto* render = getComponent<PokemonRenderComponent>(entity);
            auto* transform = getComponent<Transform>(entity);
            
            auto key = std::make_pair(render->speciesName, render->currentLOD);
            
            InstanceData instance;
            instance.modelMatrix = transform->getMatrix();
            instance.colorTint = glm::vec4(render->colorTint, 1.0f);
            instance.animationTime = render->animationTime;
            instance.textureIndex = render->isShiny ? 1 : 0;
            
            batches[key].push_back(instance);
        }
        
        // Render each batch with instancing
        for (const auto& [key, instances] : batches) {
            renderInstancedPokemon(key.first, key.second, instances);
        }
    }
};
```

## Part 6: Shader Setup for Pokemon Rendering

```glsl
// shaders/pokemon.vert
#version 450

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inJointIndices;
layout(location = 4) in vec4 inJointWeights;

// Instance attributes (for instanced rendering)
layout(location = 5) in mat4 inInstanceMatrix;
layout(location = 9) in vec4 inColorTint;
layout(location = 10) in float inAnimationTime;

// Uniforms
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float time;
} ubo;

layout(set = 1, binding = 0) uniform SkeletalAnimation {
    mat4 boneMatrices[64];
} skeletal;

// Outputs
layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec4 fragColorTint;

void main() {
    // Skeletal animation
    mat4 skinMatrix = 
        inJointWeights.x * skeletal.boneMatrices[int(inJointIndices.x)] +
        inJointWeights.y * skeletal.boneMatrices[int(inJointIndices.y)] +
        inJointWeights.z * skeletal.boneMatrices[int(inJointIndices.z)] +
        inJointWeights.w * skeletal.boneMatrices[int(inJointIndices.w)];
    
    vec4 skinnedPos = skinMatrix * vec4(inPosition, 1.0);
    vec4 skinnedNormal = skinMatrix * vec4(inNormal, 0.0);
    
    // Apply instance transform
    vec4 worldPos = inInstanceMatrix * skinnedPos;
    
    // Add subtle idle animation (breathing/swaying)
    float breathe = sin(ubo.time * 2.0 + inInstanceMatrix[3][0]) * 0.01;
    worldPos.y += breathe;
    
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    fragPos = worldPos.xyz;
    fragNormal = normalize((inInstanceMatrix * skinnedNormal).xyz);
    fragTexCoord = inTexCoord;
    fragColorTint = inColorTint;
}
```

## Testing Your Pokemon Models

```cpp
TEST_CASE("Pokemon Model Loading", "[models]") {
    PokemonModelLoader loader(device, physicalDevice, commandPool, queue);
    
    SECTION("Load Pikachu model") {
        auto model = loader.loadPokemon("pikachu");
        
        REQUIRE(model != nullptr);
        CHECK(model->name == "pikachu");
        CHECK(model->lods[0].vertices.size() > 0);
        CHECK(model->animations.find("idle") != model->animations.end());
    }
    
    SECTION("LOD levels have decreasing poly counts") {
        auto model = loader.loadPokemon("charizard");
        
        for (int i = 0; i < 3; ++i) {
            CHECK(model->lods[i].indices.size() > model->lods[i+1].indices.size());
        }
    }
    
    SECTION("Shiny textures exist") {
        auto model = loader.loadPokemon("bulbasaur");
        
        CHECK(model->shinyTextures.diffuse != VK_NULL_HANDLE);
        CHECK(model->normalTextures.diffuse != VK_NULL_HANDLE);
    }
}
```

## Directory Structure

```
assets/
├── models/
│   └── creatures/
│       ├── pikachu/
│       │   ├── metadata.json
│       │   ├── model_lod0.gltf
│       │   ├── model_lod1.gltf
│       │   ├── model_lod2.gltf
│       │   ├── model_lod3.gltf
│       │   ├── textures/
│       │   │   ├── diffuse.png
│       │   │   ├── diffuse_shiny.png
│       │   │   ├── normal.png
│       │   │   ├── metallic.png
│       │   │   └── roughness.png
│       │   └── animations/
│       │       ├── idle.gltf
│       │       ├── walk.gltf
│       │       ├── run.gltf
│       │       └── attack.gltf
│       ├── charizard/
│       │   └── ...
│       └── bulbasaur/
│           └── ...
```

## Performance Targets

- **Model Loading**: < 100ms per Pokemon (first load)
- **Cached Access**: < 1ms
- **LOD Switching**: Seamless, no visible pop
- **Batch Rendering**: 100+ Pokemon at 60 FPS
- **Memory per Pokemon**: ~5-10MB (all LODs + textures)
- **Animation Blending**: < 0.1ms per Pokemon

## Next Steps

1. **Run the Blender export script** on all your .blend files
2. **Integrate the PokemonModelLoader** into your existing ModelLoader
3. **Add the PokemonRenderComponent** to your ECS
4. **Test with a few Pokemon** before doing the full batch
5. **Optimize with texture atlases** if needed for performance

This pipeline handles everything from your Blender files to optimized in-game rendering with LODs, animations, and shiny variants!