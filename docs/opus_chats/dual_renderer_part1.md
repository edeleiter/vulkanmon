# Part 1: Dual Renderer Engine Architecture & Core Systems

## Concept Overview

**Vision**: A game engine that seamlessly transitions between **photorealistic 3D** (modern timeline) and **authentic 2D pixel art** (past timeline) as core gameplay mechanics, not just visual flair.

**Narrative Integration**: Players travel through time, experiencing the same world in different eras with era-appropriate visual styles and gameplay mechanics.

---

## Engine Architecture: Unified Dual Renderer

### Core Architecture Strategy
```cpp
class DualModeGameEngine {
private:
    // Shared systems (work for both modes)
    std::unique_ptr<SceneManager> scene_manager;
    std::unique_ptr<AudioSystem> audio_system;
    std::unique_ptr<InputSystem> input_system;
    std::unique_ptr<GameLogic> game_logic;
    
    // Rendering systems (mode-specific)
    std::unique_ptr<PhotorealisticRenderer> modern_renderer;  // 3D RT + Nanite
    std::unique_ptr<PixelArtRenderer> retro_renderer;        // 2D sprite-based
    
    // Current state
    RenderingMode current_mode;
    bool transitioning = false;
    
public:
    enum class RenderingMode {
        PHOTOREALISTIC_3D,  // Modern timeline
        PIXEL_ART_2D,       // Past timeline
        TRANSITIONING       // During time travel sequences
    };
    
    void Update(float delta_time) {
        // Shared game logic updates
        game_logic->Update(delta_time);
        scene_manager->Update(delta_time);
        
        if (transitioning) {
            UpdateTransition(delta_time);
        }
    }
    
    void Render() {
        switch (current_mode) {
            case RenderingMode::PHOTOREALISTIC_3D:
                modern_renderer->RenderFrame(scene_manager->GetSceneData());
                break;
                
            case RenderingMode::PIXEL_ART_2D:
                retro_renderer->RenderFrame(scene_manager->Get2DSceneData());
                break;
                
            case RenderingMode::TRANSITIONING:
                RenderTransitionEffect();
                break;
        }
    }
};
```

---

## Shared Scene Representation

### Universal Scene Data Structure
```cpp
class UniversalScene {
private:
    // Abstract scene representation that works for both renderers
    struct SceneObject {
        ObjectID id;
        Vector3 world_position;        // 3D position
        ObjectType type;               // Creature, environment, item, etc.
        
        // 3D representation data
        MeshHandle mesh_3d;
        MaterialHandle material_3d;
        AnimationData animation_3d;
        
        // 2D representation data  
        SpriteHandle sprite_2d;
        SpriteAnimation animation_2d;
        uint32_t sprite_layer;         // Z-order for 2D
        
        // Shared properties
        BoundingBox bounds;
        bool is_interactive;
        GameplayProperties gameplay_props;
    };
    
    std::vector<SceneObject> scene_objects;
    TerrainData terrain_3d;
    TilemapData tilemap_2d;
    
public:
    // Generate rendering data for specific mode
    PhotorealisticSceneData GetModernSceneData() const {
        PhotorealisticSceneData data;
        for (const auto& obj : scene_objects) {
            if (obj.mesh_3d.IsValid()) {
                data.renderable_objects.push_back({
                    .mesh = obj.mesh_3d,
                    .material = obj.material_3d,
                    .transform = obj.GetTransform3D(),
                    .animation = obj.animation_3d
                });
            }
        }
        data.terrain = terrain_3d;
        return data;
    }
    
    PixelArtSceneData GetRetroSceneData() const {
        PixelArtSceneData data;
        for (const auto& obj : scene_objects) {
            if (obj.sprite_2d.IsValid()) {
                Vector2 pos_2d = ProjectTo2D(obj.world_position);
                data.sprites.push_back({
                    .sprite = obj.sprite_2d,
                    .position = pos_2d,
                    .layer = obj.sprite_layer,
                    .animation = obj.animation_2d
                });
            }
        }
        data.tilemap = tilemap_2d;
        return data;
    }
};
```

### Asset Duality System
```cpp
class DualAssetManager {
private:
    struct DualAsset {
        // Modern 3D version
        MeshAsset modern_mesh;
        PBRMaterial modern_material;
        SkeletalAnimation modern_animation;
        
        // Retro 2D version
        SpriteAsset retro_sprite;
        SpriteSheet retro_spritesheet;
        PixelAnimation retro_animation;
        
        // Mapping information
        std::string base_name;         // "pikachu", "oak_tree", etc.
        Vector3 collision_bounds;      // Shared collision geometry
    };
    
    std::unordered_map<std::string, DualAsset> assets;
    
public:
    void LoadDualAsset(const std::string& name) {
        DualAsset asset;
        
        // Load modern assets
        asset.modern_mesh = LoadMesh(name + "_3d.mesh");
        asset.modern_material = LoadPBRMaterial(name + "_3d.mat");
        asset.modern_animation = LoadAnimation(name + "_3d.anim");
        
        // Load retro assets
        asset.retro_sprite = LoadSprite(name + "_2d.spr");
        asset.retro_spritesheet = LoadSpriteSheet(name + "_2d.png");
        asset.retro_animation = LoadPixelAnimation(name + "_2d.anim");
        
        asset.base_name = name;
        assets[name] = asset;
    }
    
    const DualAsset& GetAsset(const std::string& name) const {
        return assets.at(name);
    }
};
```

---

## Memory Management Strategy

### Challenge 1: Memory Management
```cpp
class DualAssetCache {
private:
    // Smart caching: only keep assets for current era + transition buffer
    std::unordered_map<std::string, ModernAsset> modern_cache;
    std::unordered_map<std::string, RetroAsset> retro_cache;
    
    RenderingMode current_mode;
    size_t max_cache_size = 512 * 1024 * 1024; // 512MB cache
    
public:
    void SetRenderingMode(RenderingMode mode) {
        if (current_mode == mode) return;
        
        current_mode = mode;
        
        if (mode == RenderingMode::PIXEL_ART_2D) {
            // Transitioning to 2D: cache retro assets, unload some modern assets
            UnloadDistantModernAssets();
            PreloadNearbyRetroAssets();
        } else {
            // Transitioning to 3D: cache modern assets, unload retro assets
            UnloadRetroAssets();
            PreloadNearbyModernAssets();
        }
    }
    
private:
    void UnloadDistantModernAssets() {
        // Keep essential modern assets for quick transitions
        // Unload detailed environment assets that won't be needed
        for (auto it = modern_cache.begin(); it != modern_cache.end();) {
            if (it->second.IsEnvironmentDetail() && 
                !it->second.IsEssentialForTransitions()) {
                it = modern_cache.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    void PreloadNearbyRetroAssets() {
        // Load 2D assets that will be needed for smooth transition
        auto nearby_objects = scene_manager->GetNearbyObjects(player_position, 50.0f);
        for (const auto& obj : nearby_objects) {
            if (obj.HasRetroVersion()) {
                LoadRetroAsset(obj.GetBaseName());
            }
        }
    }
};
```

---

## Core System Integration Points

### Shared Game Logic Layer
```cpp
class SharedGameLogic {
private:
    // Era-agnostic game state
    PlayerData player_data;
    std::vector<CreatureData> party;
    WorldState world_state;
    QuestSystem quest_system;
    
public:
    void Update(float delta_time) {
        // Update core game systems that work in both eras
        quest_system.Update(delta_time);
        UpdateCreatureAI(delta_time);
        UpdateWorldEvents(delta_time);
        
        // Process input (mode-specific handling)
        ProcessInput(input_system->GetCurrentInput());
    }
    
private:
    void ProcessInput(const InputState& input) {
        if (current_mode == RenderingMode::PIXEL_ART_2D) {
            // Process grid-based movement for 2D mode
            ProcessGridMovement(input);
        } else {
            // Process free movement for 3D mode
            ProcessFreeMovement(input);
        }
    }
    
    void ProcessGridMovement(const InputState& input) {
        // Classic Pokemon-style movement
        if (input.IsPressed(InputKey::UP) && !is_moving) {
            StartMove(Direction::NORTH);
        }
        // ... other directions
    }
    
    void ProcessFreeMovement(const InputState& input) {
        // Modern 3D movement
        Vector3 movement_vector;
        if (input.IsHeld(InputKey::W)) movement_vector.z += 1.0f;
        if (input.IsHeld(InputKey::S)) movement_vector.z -= 1.0f;
        if (input.IsHeld(InputKey::A)) movement_vector.x -= 1.0f;
        if (input.IsHeld(InputKey::D)) movement_vector.x += 1.0f;
        
        ApplyContinuousMovement(movement_vector);
    }
};
```

### Cross-System Communication
```cpp
class EventBus {
private:
    std::vector<std::function<void(const GameEvent&)>> event_handlers;
    
public:
    void Subscribe(std::function<void(const GameEvent&)> handler) {
        event_handlers.push_back(handler);
    }
    
    void Publish(const GameEvent& event) {
        for (const auto& handler : event_handlers) {
            handler(event);
        }
    }
    
    // Example events
    void PublishModeTransition(RenderingMode from, RenderingMode to) {
        ModeTransitionEvent event;
        event.from_mode = from;
        event.to_mode = to;
        event.timestamp = GetCurrentTime();
        
        Publish(event);
    }
    
    void PublishCreatureEncounter(const CreatureData& creature) {
        CreatureEncounterEvent event;
        event.creature = creature;
        event.location = player->GetPosition();
        
        Publish(event);
    }
};
```

This covers the foundational architecture and core systems. The key insight is that you need a **unified scene representation** that can generate appropriate data for either renderer, plus smart memory management to handle the dual asset requirements.