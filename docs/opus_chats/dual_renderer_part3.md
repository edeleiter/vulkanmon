# Part 3: Transition System & Cross-Era Gameplay

## Time Travel Transition Effects

### Core Transition System
```cpp
class TimeTransitionRenderer {
private:
    enum class TransitionType {
        DISSOLVE,           // Gradual fade between eras
        SPIRAL_TIME,        // Spiral time vortex effect  
        GLITCH_REWIND,      // Digital glitch/rewind effect
        PIXEL_EXPLOSION     // 3D world explodes into pixels
    };
    
    float transition_progress = 0.0f; // 0 = 3D, 1 = 2D
    TransitionType current_transition;
    
    // Transition render targets
    RenderTarget transition_3d_rt;
    RenderTarget transition_2d_rt;
    RenderTarget composite_rt;
    
public:
    void StartTransition(TransitionType type, float duration) {
        current_transition = type;
        transition_progress = 0.0f;
        
        // Prepare transition by rendering both versions
        Render3DVersion();
        Render2DVersion();
    }
    
    void UpdateTransition(float delta_time, float transition_speed) {
        transition_progress += delta_time * transition_speed;
        transition_progress = std::clamp(transition_progress, 0.0f, 1.0f);
    }
    
    void RenderTransitionFrame() {
        composite_rt.Bind();
        
        switch (current_transition) {
            case TransitionType::DISSOLVE:
                RenderDissolveTransition();
                break;
            case TransitionType::SPIRAL_TIME:
                RenderSpiralTransition();
                break;
            case TransitionType::GLITCH_REWIND:
                RenderGlitchTransition();
                break;
            case TransitionType::PIXEL_EXPLOSION:
                RenderPixelExplosionTransition();
                break;
        }
    }
    
private:
    void RenderPixelExplosionTransition() {
        // Most complex transition: 3D world "explodes" into 2D pixels
        
        if (transition_progress < 0.5f) {
            // First half: 3D world breaks apart
            float break_progress = transition_progress * 2.0f;
            
            pixel_explosion_shader.Bind();
            pixel_explosion_shader.SetTexture("scene_3d", transition_3d_rt.GetColorTexture());
            pixel_explosion_shader.SetFloat("break_progress", break_progress);
            pixel_explosion_shader.SetFloat("time", GetTime());
            
            // Shader breaks 3D image into flying pixel chunks
            RenderFullscreenQuad();
            
        } else {
            // Second half: pixels coalesce into 2D image
            float coalesce_progress = (transition_progress - 0.5f) * 2.0f;
            
            pixel_coalesce_shader.Bind();
            pixel_coalesce_shader.SetTexture("scene_2d", transition_2d_rt.GetColorTexture());
            pixel_coalesce_shader.SetFloat("coalesce_progress", coalesce_progress);
            
            RenderFullscreenQuad();
        }
    }
    
    void RenderDissolveTransition() {
        // Simple cross-fade with animated dissolve pattern
        dissolve_shader.Bind();
        dissolve_shader.SetTexture("scene_3d", transition_3d_rt.GetColorTexture());
        dissolve_shader.SetTexture("scene_2d", transition_2d_rt.GetColorTexture());
        dissolve_shader.SetTexture("dissolve_pattern", dissolve_noise_texture);
        dissolve_shader.SetFloat("progress", transition_progress);
        
        RenderFullscreenQuad();
    }
    
    void RenderSpiralTransition() {
        // Spiral time vortex effect
        spiral_shader.Bind();
        spiral_shader.SetTexture("scene_3d", transition_3d_rt.GetColorTexture());
        spiral_shader.SetTexture("scene_2d", transition_2d_rt.GetColorTexture());
        spiral_shader.SetFloat("progress", transition_progress);
        spiral_shader.SetFloat("time", GetTime());
        spiral_shader.SetVector2("center", Vector2(0.5f, 0.5f)); // Screen center
        
        RenderFullscreenQuad();
    }
};
```

### Transition Shaders
```glsl
// pixel_explosion.frag - 3D world breaking into pixels
#version 450 core

in vec2 frag_texcoord;
out vec4 out_color;

uniform sampler2D scene_3d;
uniform float break_progress;
uniform float time;

// Noise function for randomized pixel behavior
float noise(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec2 uv = frag_texcoord;
    
    // Calculate pixel grid (simulate low-res pixels)
    vec2 pixel_size = 1.0 / vec2(240.0, 160.0); // GBA resolution
    vec2 pixel_coord = floor(uv / pixel_size) * pixel_size;
    
    // Sample the 3D scene
    vec4 scene_color = texture(scene_3d, pixel_coord);
    
    // Calculate explosion offset for this pixel
    float pixel_hash = noise(pixel_coord + vec2(0.1));
    vec2 explosion_dir = normalize(vec2(
        noise(pixel_coord) * 2.0 - 1.0,
        noise(pixel_coord + vec2(0.5)) * 2.0 - 1.0
    ));
    
    // Animate pixel explosion
    float explosion_distance = break_progress * break_progress * 0.5;
    vec2 exploded_offset = explosion_dir * explosion_distance * pixel_hash;
    
    // Apply offset
    vec2 exploded_uv = pixel_coord + exploded_offset;
    
    // Fade out as pixels fly away
    float alpha = 1.0 - break_progress;
    
    // Only render if pixel is still on screen
    if (exploded_uv.x >= 0.0 && exploded_uv.x <= 1.0 && 
        exploded_uv.y >= 0.0 && exploded_uv.y <= 1.0) {
        
        vec4 color = texture(scene_3d, exploded_uv);
        out_color = vec4(color.rgb, color.a * alpha);
    } else {
        out_color = vec4(0.0);
    }
}
```

```glsl
// spiral_transition.frag - Time vortex effect
#version 450 core

in vec2 frag_texcoord;
out vec4 out_color;

uniform sampler2D scene_3d;
uniform sampler2D scene_2d;
uniform float progress;
uniform float time;
uniform vec2 center;

void main() {
    vec2 uv = frag_texcoord;
    vec2 center_offset = uv - center;
    
    // Calculate spiral parameters
    float distance = length(center_offset);
    float angle = atan(center_offset.y, center_offset.x);
    
    // Animated spiral distortion
    float spiral_strength = progress * 10.0;
    float spiral_speed = time * 2.0;
    angle += distance * spiral_strength + spiral_speed;
    
    // Create spiral UV coordinates
    vec2 spiral_uv = center + distance * vec2(cos(angle), sin(angle));
    
    // Sample both scenes with spiral distortion
    vec4 color_3d = texture(scene_3d, spiral_uv);
    vec4 color_2d = texture(scene_2d, uv);
    
    // Mix based on distance from center and progress
    float mix_factor = smoothstep(0.0, 1.0, distance + progress - 0.5);
    
    out_color = mix(color_3d, color_2d, mix_factor);
}
```

---

## Cross-Era Gameplay Systems

### Time Period Game State Manager
```cpp
class TimePeriodManager {
public:
    enum class TimePeriod {
        MODERN_ERA,         // 3D photorealistic
        CLASSIC_ERA         // 2D pixel art
    };
    
private:
    TimePeriod current_period;
    
    // Era-specific gameplay rules
    struct EraRules {
        uint32_t max_party_size;        // 6 in classic, maybe 4 in modern
        bool has_abilities;             // Modern mechanic
        bool has_held_items;            // Modern mechanic  
        bool has_breeding;              // Modern mechanic
        uint32_t move_count_limit;      // 4 in classic era
        bool allows_running;            // Running shoes in modern era
        CameraMode camera_mode;         // Top-down vs 3rd person
        BattleStyle battle_style;       // Turn-based vs real-time
    };
    
    std::unordered_map<TimePeriod, EraRules> era_rules;
    
public:
    void Initialize() {
        // Set up era rules
        era_rules[TimePeriod::CLASSIC_ERA] = {
            .max_party_size = 6,
            .has_abilities = false,
            .has_held_items = false,
            .has_breeding = false,
            .move_count_limit = 4,
            .allows_running = false,
            .camera_mode = CameraMode::TOP_DOWN_ORTHOGRAPHIC,
            .battle_style = BattleStyle::TURN_BASED_MENU
        };
        
        era_rules[TimePeriod::MODERN_ERA] = {
            .max_party_size = 4,
            .has_abilities = true,
            .has_held_items = true,
            .has_breeding = true,
            .move_count_limit = 8,
            .allows_running = true,
            .camera_mode = CameraMode::THIRD_PERSON_3D,
            .battle_style = BattleStyle::REAL_TIME_3D
        };
    }
    
    void SwitchToPeriod(TimePeriod new_period) {
        if (current_period == new_period) return;
        
        current_period = new_period;
        
        // Update gameplay systems based on era
        ApplyEraRules(era_rules[new_period]);
        
        // Trigger renderer switch
        engine->SetRenderingMode(
            new_period == TimePeriod::MODERN_ERA 
                ? RenderingMode::PHOTOREALISTIC_3D 
                : RenderingMode::PIXEL_ART_2D
        );
        
        // Convert creatures to era-appropriate versions
        ConvertCreaturesToEra(new_period);
    }
    
private:
    void ApplyEraRules(const EraRules& rules) {
        // Update game systems to match the era
        party_system->SetMaxPartySize(rules.max_party_size);
        move_system->SetMoveCountLimit(rules.move_count_limit);
        camera_system->SetCameraMode(rules.camera_mode);
        battle_system->SetBattleStyle(rules.battle_style);
        
        // Enable/disable modern features
        ability_system->SetEnabled(rules.has_abilities);
        item_system->SetHeldItemsEnabled(rules.has_held_items);
        breeding_system->SetEnabled(rules.has_breeding);
        movement_system->SetRunningEnabled(rules.allows_running);
    }
    
    void ConvertCreaturesToEra(TimePeriod era) {
        // Convert all creatures in party to era-appropriate versions
        auto& party = party_system->GetParty();
        for (auto& creature : party) {
            creature = cross_era_mapper->ConvertCreature(creature, era);
        }
    }
};
```

### Cross-Era Asset Mapping System
```cpp
class CrossEraMapper {
private:
    struct CreatureMappings {
        // Visual mappings
        std::string modern_3d_mesh;
        std::string retro_2d_sprite;
        
        // Gameplay mappings
        std::vector<std::string> modern_moves;    // Full modern moveset
        std::vector<std::string> classic_moves;   // Limited classic moveset
        
        CreatureStats modern_stats;              // Modern stat system
        CreatureStats classic_stats;             // Classic stat system
        
        // Evolution differences
        std::vector<std::string> modern_evolution_chain;
        std::vector<std::string> classic_evolution_chain;
    };
    
    std::unordered_map<std::string, CreatureMappings> creature_mappings;
    
public:
    void LoadCreatureMappings() {
        // Example: Pikachu mapping
        CreatureMappings pikachu_mapping;
        pikachu_mapping.modern_3d_mesh = "pikachu_3d.mesh";
        pikachu_mapping.retro_2d_sprite = "pikachu_2d.spr";
        
        // Modern era: Full moveset with abilities
        pikachu_mapping.modern_moves = {
            "thunderbolt", "quick_attack", "thunder_wave", "double_team",
            "agility", "thunder", "light_screen", "seismic_toss",
            "volt_tackle", "discharge" // Modern moves
        };
        
        // Classic era: Limited Gen 1 moveset
        pikachu_mapping.classic_moves = {
            "thunderbolt", "quick_attack", "thunder_wave", "double_team"
        };
        
        // Stats might be different between eras (balance changes over time)
        pikachu_mapping.modern_stats = {55, 40, 50, 50, 90}; // HP, ATK, DEF, SP, SPD
        pikachu_mapping.classic_stats = {35, 55, 30, 50, 90}; // Classic stat distribution
        
        // Evolution chains might differ
        pikachu_mapping.modern_evolution_chain = {"pichu", "pikachu", "raichu", "alolan_raichu"};
        pikachu_mapping.classic_evolution_chain = {"pikachu", "raichu"}; // No baby forms or variants
        
        creature_mappings["pikachu"] = pikachu_mapping;
        
        // Load more creatures...
        LoadCharizardMappings();
        LoadMewtwoMappings();
        // etc.
    }
    
    CreatureData ConvertCreature(const CreatureData& creature, TimePeriod era) {
        const auto& mapping = creature_mappings[creature.species];
        
        CreatureData converted = creature; // Copy base data
        
        if (era == TimePeriod::MODERN_ERA) {
            converted.visual_asset = mapping.modern_3d_mesh;
            converted.available_moves = FilterKnownMoves(creature.known_moves, mapping.modern_moves);
            converted.base_stats = mapping.modern_stats;
            converted.evolution_chain = mapping.modern_evolution_chain;
            
        } else {
            converted.visual_asset = mapping.retro_2d_sprite;
            converted.available_moves = FilterKnownMoves(creature.known_moves, mapping.classic_moves);
            converted.base_stats = mapping.classic_stats;
            converted.evolution_chain = mapping.classic_evolution_chain;
            
            // Remove modern-only features
            converted.ability = ""; // No abilities in classic era
            converted.held_item = ""; // No held items
            converted.nature = ""; // No natures
        }
        
        return converted;
    }
    
private:
    std::vector<std::string> FilterKnownMoves(const std::vector<std::string>& known_moves,
                                            const std::vector<std::string>& era_moves) {
        std::vector<std::string> filtered_moves;
        
        for (const auto& move : known_moves) {
            if (std::find(era_moves.begin(), era_moves.end(), move) != era_moves.end()) {
                filtered_moves.push_back(move);
            }
        }
        
        return filtered_moves;
    }
    
    void LoadCharizardMappings() {
        CreatureMappings charizard_mapping;
        charizard_mapping.modern_3d_mesh = "charizard_3d.mesh";
        charizard_mapping.retro_2d_sprite = "charizard_2d.spr";
        
        // Modern: Mega evolutions, different typing options
        charizard_mapping.modern_evolution_chain = {
            "charmander", "charmeleon", "charizard", "mega_charizard_x", "mega_charizard_y"
        };
        
        // Classic: Simple three-stage evolution
        charizard_mapping.classic_evolution_chain = {
            "charmander", "charmeleon", "charizard"
        };
        
        creature_mappings["charizard"] = charizard_mapping;
    }
};
```

---

## Animation Synchronization System

### Dual Animation Controller
```cpp
class DualAnimationSystem {
private:
    struct SyncedAnimation {
        std::string animation_name;
        float current_time;
        float duration;
        bool is_looping;
        
        // Modern 3D animation
        SkeletalAnimationHandle modern_anim;
        
        // Retro 2D sprite animation  
        SpriteAnimationHandle retro_anim;
        
        // Sync data
        std::vector<float> sync_points; // Key moments that must align
    };
    
    std::unordered_map<std::string, SyncedAnimation> active_animations;
    
public:
    void PlaySyncedAnimation(const std::string& creature_id, 
                           const std::string& animation_name) {
        SyncedAnimation sync_anim;
        sync_anim.animation_name = animation_name;
        sync_anim.current_time = 0.0f;
        
        // Load both versions of the animation
        sync_anim.modern_anim = LoadModernAnimation(creature_id, animation_name);
        sync_anim.retro_anim = LoadRetroAnimation(creature_id, animation_name);
        
        // Sync timing (use the longer duration as base)
        float modern_duration = sync_anim.modern_anim->GetDuration();
        float retro_duration = sync_anim.retro_anim->GetDuration();
        sync_anim.duration = std::max(modern_duration, retro_duration);
        
        // Define sync points (key moments that must align between eras)
        if (animation_name == "attack") {
            sync_anim.sync_points = {0.0f, 0.3f, 0.7f, 1.0f}; // Start, windup, hit, recovery
        } else if (animation_name == "idle") {
            sync_anim.sync_points = {0.0f, 0.5f}; // Breathing cycle
        }
        
        active_animations[creature_id] = sync_anim;
    }
    
    void Update(float delta_time) {
        for (auto& [creature_id, anim] : active_animations) {
            anim.current_time += delta_time;
            
            // Handle looping
            if (anim.current_time >= anim.duration) {
                if (anim.is_looping) {
                    anim.current_time = fmod(anim.current_time, anim.duration);
                } else {
                    // Animation finished
                    active_animations.erase(creature_id);
                    continue;
                }
            }
            
            // Update both animation systems with synchronized time
            float normalized_time = anim.current_time / anim.duration;
            
            // Apply sync point corrections if needed
            normalized_time = ApplySyncCorrection(normalized_time, anim.sync_points);
            
            anim.modern_anim->SetNormalizedTime(normalized_time);
            anim.retro_anim->SetNormalizedTime(normalized_time);
        }
    }
    
private:
    float ApplySyncCorrection(float time, const std::vector<float>& sync_points) {
        // Ensure key animation moments align perfectly between eras
        for (size_t i = 0; i < sync_points.size() - 1; ++i) {
            if (time >= sync_points[i] && time <= sync_points[i + 1]) {
                // We're between two sync points - apply smooth interpolation
                float segment_start = sync_points[i];
                float segment_end = sync_points[i + 1];
                float segment_progress = (time - segment_start) / (segment_end - segment_start);
                
                // Use easing to make sync points more pronounced
                segment_progress = EaseInOutCubic(segment_progress);
                
                return segment_start + segment_progress * (segment_end - segment_start);
            }
        }
        
        return time; // No correction needed
    }
    
    float EaseInOutCubic(float t) {
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }
};
```

---

## Narrative Integration Framework

### Story-Driven Era Transitions
```cpp
class NarrativeTimeManager {
private:
    enum class TransitionReason {
        STORY_CUTSCENE,     // Scripted story moment
        PLAYER_CHOICE,      // Player activates time device
        COMBAT_SPECIAL,     // Special battle triggers transition
        EXPLORATION_TRIGGER // Environmental story trigger
    };
    
    struct TimeEvent {
        std::string event_id;
        TransitionReason reason;
        TimePeriod target_era;
        TransitionType visual_effect;
        std::string narrative_context;
        bool is_reversible;
    };
    
    std::vector<TimeEvent> story_events;
    std::string current_narrative_context;
    
public:
    void RegisterStoryEvent(const std::string& event_id,
                           TimePeriod target_era,
                           const std::string& narrative_context) {
        TimeEvent event;
        event.event_id = event_id;
        event.reason = TransitionReason::STORY_CUTSCENE;
        event.target_era = target_era;
        event.visual_effect = TransitionType::SPIRAL_TIME; // Default
        event.narrative_context = narrative_context;
        event.is_reversible = false; // Story events are usually one-way
        
        story_events.push_back(event);
    }
    
    void TriggerStoryTransition(const std::string& event_id) {
        auto event_it = std::find_if(story_events.begin(), story_events.end(),
            [&event_id](const TimeEvent& event) {
                return event.event_id == event_id;
            });
            
        if (event_it != story_events.end()) {
            ExecuteTimeTransition(*event_it);
        }
    }
    
private:
    void ExecuteTimeTransition(const TimeEvent& event) {
        // Update narrative context
        current_narrative_context = event.narrative_context;
        
        // Show narrative text if needed
        if (!event.narrative_context.empty()) {
            ui_system->ShowNarrativeText(event.narrative_context);
        }
        
        // Start visual transition
        transition_renderer->StartTransition(event.visual_effect, 3.0f);
        
        // Switch era
        time_period_manager->SwitchToPeriod(event.target_era);
        
        // Log story progression
        story_log.RecordTimeTransition(event.event_id, event.target_era);
    }
};
```

### Era-Specific Story Content
```cpp
class EraContentManager {
private:
    struct EraContent {
        std::vector<std::string> available_npcs;
        std::vector<std::string> available_locations;
        std::vector<std::string> era_specific_quests;
        std::map<std::string, std::string> dialogue_variants;
    };
    
    std::unordered_map<TimePeriod, EraContent> era_content;
    
public:
    void LoadEraContent() {
        // Modern era content
        EraContent modern_content;
        modern_content.available_npcs = {
            "professor_oak_modern", "gym_leader_brock_modern", "champion_lance_modern"
        };
        modern_content.available_locations = {
            "pallet_town_modern", "viridian_forest_modern", "mt_silver_modern"
        };
        modern_content.era_specific_quests = {
            "investigate_time_anomaly", "restore_ancient_pokemon", "master_mega_evolution"
        };
        modern_content.dialogue_variants = {
            {"oak_greeting", "Welcome! I've been studying these time distortions..."},
            {"brock_intro", "My rock-types have adapted to modern battle techniques!"}
        };
        era_content[TimePeriod::MODERN_ERA] = modern_content;
        
        // Classic era content
        EraContent classic_content;
        classic_content.available_npcs = {
            "professor_oak_classic", "gym_leader_brock_classic", "champion_lance_classic"
        };
        classic_content.available_locations = {
            "pallet_town_classic", "viridian_forest_classic", "mt_silver_classic"
        };
        classic_content.era_specific_quests = {
            "become_champion", "complete_pokedex", "defeat_elite_four"
        };
        classic_content.dialogue_variants = {
            {"oak_greeting", "Hello there! Welcome to the world of Pokemon!"},
            {"brock_intro", "I'm Brock! I use rock-type Pokemon!"}
        };
        era_content[TimePeriod::CLASSIC_ERA] = classic_content;
    }
    
    std::vector<std::string> GetAvailableQuests(TimePeriod era) {
        return era_content[era].era_specific_quests;
    }
    
    std::string GetDialogue(const std::string& dialogue_id, TimePeriod era) {
        const auto& variants = era_content[era].dialogue_variants;
        if (variants.find(dialogue_id) != variants.end()) {
            return variants.at(dialogue_id);
        }
        return "..."; // Fallback
    }
};
```

This covers the transition system, cross-era gameplay mechanics, and narrative integration. The key innovation is having **synchronized systems** that maintain gameplay continuity while completely changing visual presentation and era-specific rules.