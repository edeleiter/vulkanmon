# Part 2: 2D Pixel Art Renderer Implementation

## Authentic Pixel Art Pipeline

### Core Pixel Art Renderer
```cpp
class PixelArtRenderer {
private:
    // Core rendering settings
    uint32_t internal_width = 240;   // GBA resolution width
    uint32_t internal_height = 160;  // GBA resolution height
    uint32_t scale_factor = 4;       // Scale up for modern displays
    
    // Rendering resources
    RenderTarget pixel_rt;           // Low-res render target
    TilemapRenderer tilemap_renderer;
    SpriteRenderer sprite_renderer;
    
    // Pixel-perfect settings
    bool pixel_perfect = true;
    FilterMode filter_mode = FilterMode::Nearest; // No filtering!
    
public:
    void Initialize() {
        // Create low-resolution render target
        pixel_rt.Create(internal_width, internal_height, TextureFormat::RGBA8);
        
        // Ensure no filtering for authentic pixel art
        pixel_rt.SetFilterMode(FilterMode::Nearest);
        pixel_rt.SetWrapMode(WrapMode::Clamp);
    }
    
    void RenderFrame(const PixelArtSceneData& scene_data) {
        // 1. Render to low-resolution buffer
        pixel_rt.Bind();
        SetViewport(0, 0, internal_width, internal_height);
        
        // 2. Clear with background color
        Clear(scene_data.background_color);
        
        // 3. Render tilemap (background)
        tilemap_renderer.RenderTilemap(scene_data.tilemap);
        
        // 4. Render sprites (sorted by layer)
        auto sorted_sprites = SortSpritesByLayer(scene_data.sprites);
        for (const auto& sprite : sorted_sprites) {
            sprite_renderer.RenderSprite(sprite);
        }
        
        // 5. Scale up to screen resolution (with nearest neighbor)
        RenderTarget screen_rt = GetScreenRenderTarget();
        BlitPixelPerfect(pixel_rt, screen_rt, scale_factor);
    }
    
private:
    void BlitPixelPerfect(const RenderTarget& source, RenderTarget& dest, 
                         uint32_t scale) {
        // Custom pixel-perfect upscaling shader
        pixel_upscale_shader.Bind();
        pixel_upscale_shader.SetTexture("source_texture", source.GetColorTexture());
        pixel_upscale_shader.SetInt("scale_factor", scale);
        
        // Render fullscreen quad with nearest neighbor sampling
        RenderFullscreenQuad();
    }
};
```

---

## Tilemap System (Like GBA Backgrounds)

### Tilemap Renderer
```cpp
class TilemapRenderer {
private:
    struct Tile {
        uint16_t tile_id;           // Index into tile atlas
        uint8_t palette_id;         // Color palette index
        bool flip_x, flip_y;        // Tile flipping
        uint8_t priority;           // Rendering priority
    };
    
    struct Tilemap {
        uint32_t width, height;     // Map dimensions in tiles
        std::vector<Tile> tiles;    // Tile data
        TileAtlas atlas;            // Shared tile graphics
    };
    
    TileAtlas tile_atlas;
    
public:
    void RenderTilemap(const Tilemap& tilemap) {
        // Render each tile as a quad
        for (uint32_t y = 0; y < tilemap.height; ++y) {
            for (uint32_t x = 0; x < tilemap.width; ++x) {
                const Tile& tile = tilemap.tiles[y * tilemap.width + x];
                
                if (tile.tile_id == 0) continue; // Empty tile
                
                // Get tile graphics from atlas
                Rectangle tile_uv = tile_atlas.GetTileUV(tile.tile_id);
                
                // Calculate screen position (8x8 tiles)
                Vector2 screen_pos = Vector2(x * 8, y * 8);
                
                // Render tile quad
                RenderTileQuad(screen_pos, tile_uv, tile.palette_id, 
                              tile.flip_x, tile.flip_y);
            }
        }
    }
    
private:
    void RenderTileQuad(Vector2 position, Rectangle uv, uint8_t palette,
                       bool flip_x, bool flip_y) {
        // Create vertices for 8x8 tile
        std::vector<SpriteVertex> vertices;
        
        Vector2 size(8, 8);
        Vector2 uv_min = uv.GetMin();
        Vector2 uv_max = uv.GetMax();
        
        // Handle flipping
        if (flip_x) std::swap(uv_min.x, uv_max.x);
        if (flip_y) std::swap(uv_min.y, uv_max.y);
        
        // Generate quad vertices
        vertices.push_back({position, uv_min, palette});
        vertices.push_back({position + Vector2(size.x, 0), Vector2(uv_max.x, uv_min.y), palette});
        vertices.push_back({position + size, uv_max, palette});
        vertices.push_back({position + Vector2(0, size.y), Vector2(uv_min.x, uv_max.y), palette});
        
        RenderQuad(vertices);
    }
};
```

### Tile Atlas System
```cpp
class TileAtlas {
private:
    Texture2D atlas_texture;
    uint32_t tile_size = 8;         // 8x8 tiles (GBA standard)
    uint32_t atlas_width, atlas_height;
    
public:
    void LoadTileAtlas(const std::string& atlas_path) {
        // Load the tile atlas image
        ImageData image_data = LoadImage(atlas_path);
        atlas_width = image_data.width;
        atlas_height = image_data.height;
        
        // Create texture with nearest neighbor filtering
        atlas_texture.Create(atlas_width, atlas_height, TextureFormat::RGBA8);
        atlas_texture.SetFilterMode(FilterMode::Nearest);
        atlas_texture.Upload(image_data.pixels);
    }
    
    Rectangle GetTileUV(uint16_t tile_id) {
        // Calculate UV coordinates for tile
        uint32_t tiles_per_row = atlas_width / tile_size;
        uint32_t tile_x = tile_id % tiles_per_row;
        uint32_t tile_y = tile_id / tiles_per_row;
        
        float u_min = (float)(tile_x * tile_size) / atlas_width;
        float v_min = (float)(tile_y * tile_size) / atlas_height;
        float u_max = (float)((tile_x + 1) * tile_size) / atlas_width;
        float v_max = (float)((tile_y + 1) * tile_size) / atlas_height;
        
        return Rectangle(u_min, v_min, u_max - u_min, v_max - v_min);
    }
};
```

---

## Authentic Sprite System

### GBA-Style Sprite Renderer
```cpp
class SpriteRenderer {
private:
    struct SpriteInstance {
        Vector2 position;
        Rectangle source_rect;      // Source rectangle in sprite sheet
        uint8_t palette_id;        // GBA-style palette system
        uint8_t priority;          // Sprite priority for layering
        bool flip_x, flip_y;       // Sprite flipping
        Vector2 scale;             // Usually (1,1) for pixel perfect
    };
    
    // GBA-style palette system (16 colors per palette)
    std::array<Color, 16> palettes[16]; // 16 palettes of 16 colors each
    
    // Sprite batching for performance
    std::vector<SpriteInstance> sprite_batch;
    Shader sprite_shader;
    
public:
    void RenderSprite(const SpriteInstance& sprite) {
        // Add to batch for rendering
        sprite_batch.push_back(sprite);
    }
    
    void FlushSprites() {
        if (sprite_batch.empty()) return;
        
        // Sort sprites by palette and texture for batching
        std::sort(sprite_batch.begin(), sprite_batch.end(), 
                 [](const SpriteInstance& a, const SpriteInstance& b) {
                     return a.palette_id < b.palette_id;
                 });
        
        // Render all sprites in batch
        sprite_shader.Bind();
        
        for (const auto& sprite : sprite_batch) {
            sprite_shader.SetTexture("sprite_sheet", current_sprite_sheet);
            sprite_shader.SetColorArray("palette", palettes[sprite.palette_id]);
            sprite_shader.SetVector2("position", sprite.position);
            sprite_shader.SetRectangle("source_rect", sprite.source_rect);
            sprite_shader.SetBool("flip_x", sprite.flip_x);
            sprite_shader.SetBool("flip_y", sprite.flip_y);
            
            RenderSpriteQuad();
        }
        
        sprite_batch.clear();
    }
    
    void LoadPalette(uint8_t palette_index, const std::array<Color, 16>& colors) {
        palettes[palette_index] = colors;
    }
    
private:
    void RenderSpriteQuad() {
        // Render a simple quad for the sprite
        // Vertices are generated in the vertex shader based on position/source_rect
        DrawArrays(PrimitiveType::TriangleStrip, 0, 4);
    }
};
```

### Sprite Animation System
```cpp
class SpriteAnimationSystem {
private:
    struct SpriteAnimation {
        std::string name;
        std::vector<Rectangle> frames;  // UV rectangles for each frame
        std::vector<float> frame_durations;
        bool is_looping;
        float total_duration;
    };
    
    struct AnimatedSprite {
        SpriteHandle sprite;
        SpriteAnimation current_animation;
        float current_time;
        uint32_t current_frame;
        bool is_playing;
    };
    
    std::unordered_map<std::string, SpriteAnimation> animations;
    std::vector<AnimatedSprite> active_sprites;
    
public:
    void LoadSpriteAnimation(const std::string& name, 
                           const std::vector<Rectangle>& frames,
                           const std::vector<float>& durations,
                           bool looping = true) {
        SpriteAnimation anim;
        anim.name = name;
        anim.frames = frames;
        anim.frame_durations = durations;
        anim.is_looping = looping;
        
        // Calculate total duration
        anim.total_duration = 0.0f;
        for (float duration : durations) {
            anim.total_duration += duration;
        }
        
        animations[name] = anim;
    }
    
    void PlayAnimation(SpriteHandle sprite, const std::string& animation_name) {
        if (animations.find(animation_name) == animations.end()) {
            return; // Animation not found
        }
        
        AnimatedSprite animated_sprite;
        animated_sprite.sprite = sprite;
        animated_sprite.current_animation = animations[animation_name];
        animated_sprite.current_time = 0.0f;
        animated_sprite.current_frame = 0;
        animated_sprite.is_playing = true;
        
        active_sprites.push_back(animated_sprite);
    }
    
    void Update(float delta_time) {
        for (auto& animated_sprite : active_sprites) {
            if (!animated_sprite.is_playing) continue;
            
            animated_sprite.current_time += delta_time;
            
            // Calculate current frame based on time
            float accumulated_time = 0.0f;
            uint32_t frame_index = 0;
            
            for (uint32_t i = 0; i < animated_sprite.current_animation.frame_durations.size(); ++i) {
                accumulated_time += animated_sprite.current_animation.frame_durations[i];
                if (animated_sprite.current_time <= accumulated_time) {
                    frame_index = i;
                    break;
                }
            }
            
            animated_sprite.current_frame = frame_index;
            
            // Handle looping
            if (animated_sprite.current_time >= animated_sprite.current_animation.total_duration) {
                if (animated_sprite.current_animation.is_looping) {
                    animated_sprite.current_time = 0.0f;
                    animated_sprite.current_frame = 0;
                } else {
                    animated_sprite.is_playing = false;
                }
            }
            
            // Update sprite's current frame
            Rectangle current_frame_uv = animated_sprite.current_animation.frames[animated_sprite.current_frame];
            animated_sprite.sprite->SetSourceRectangle(current_frame_uv);
        }
    }
};
```

---

## Pixel Art Shaders

### Pixel Art Vertex Shader
```glsl
// pixel_art_vertex.glsl
#version 450 core

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in uint in_palette_id;

layout(location = 0) out vec2 frag_texcoord;
layout(location = 1) flat out uint frag_palette_id;

uniform mat4 projection_matrix;  // Orthographic projection

void main() {
    gl_Position = projection_matrix * vec4(in_position, 0.0, 1.0);
    frag_texcoord = in_texcoord;
    frag_palette_id = in_palette_id;
}
```

### Pixel Art Fragment Shader with Palette System
```glsl
// pixel_art_fragment.glsl
#version 450 core

layout(location = 0) in vec2 frag_texcoord;
layout(location = 1) flat in uint frag_palette_id;

layout(location = 0) out vec4 out_color;

uniform sampler2D sprite_texture;

// GBA-style palette system
uniform vec4 palettes[16][16]; // 16 palettes, 16 colors each

void main() {
    // Sample the sprite texture (this contains palette indices, not colors)
    vec4 texture_sample = texture(sprite_texture, frag_texcoord);
    
    // Extract palette index from red channel (0-15)
    uint color_index = uint(texture_sample.r * 15.0);
    
    // Look up actual color from palette
    vec4 final_color = palettes[frag_palette_id][color_index];
    
    // Handle transparency (color index 0 is typically transparent)
    if (color_index == 0u) {
        discard;
    }
    
    out_color = final_color;
}
```

### Pixel Perfect Upscaling Shader
```glsl
// pixel_upscale.frag
#version 450 core

in vec2 frag_texcoord;
out vec4 out_color;

uniform sampler2D source_texture;
uniform int scale_factor;

void main() {
    // Calculate the exact pixel we're sampling
    vec2 texture_size = textureSize(source_texture, 0);
    vec2 pixel_coord = floor(frag_texcoord * texture_size);
    
    // Sample from the center of the pixel (no filtering)
    vec2 sample_uv = (pixel_coord + 0.5) / texture_size;
    
    out_color = texture(source_texture, sample_uv);
}
```

---

## Performance Optimizations for Pixel Art

### Sprite Batching System
```cpp
class SpriteBatcher {
private:
    struct SpriteBatch {
        TextureHandle texture;
        uint8_t palette_id;
        std::vector<SpriteVertex> vertices;
    };
    
    std::vector<SpriteBatch> batches;
    uint32_t max_sprites_per_batch = 1000;
    
public:
    void AddSprite(const SpriteInstance& sprite) {
        // Find existing batch or create new one
        SpriteBatch* target_batch = nullptr;
        
        for (auto& batch : batches) {
            if (batch.texture == sprite.texture && 
                batch.palette_id == sprite.palette_id &&
                batch.vertices.size() < max_sprites_per_batch * 4) {
                target_batch = &batch;
                break;
            }
        }
        
        if (!target_batch) {
            // Create new batch
            batches.push_back(SpriteBatch{
                .texture = sprite.texture,
                .palette_id = sprite.palette_id,
                .vertices = {}
            });
            target_batch = &batches.back();
        }
        
        // Add sprite vertices to batch
        AddSpriteVertices(*target_batch, sprite);
    }
    
    void RenderBatches() {
        for (const auto& batch : batches) {
            // Set texture and palette
            SetTexture(batch.texture);
            SetPalette(batch.palette_id);
            
            // Render all sprites in this batch
            RenderVertices(batch.vertices);
        }
        
        // Clear batches for next frame
        batches.clear();
    }
    
private:
    void AddSpriteVertices(SpriteBatch& batch, const SpriteInstance& sprite) {
        // Generate 4 vertices for sprite quad
        Vector2 pos = sprite.position;
        Rectangle src = sprite.source_rect;
        Vector2 size = sprite.GetSize();
        
        batch.vertices.push_back({pos, src.GetMin(), sprite.palette_id});
        batch.vertices.push_back({pos + Vector2(size.x, 0), Vector2(src.GetMax().x, src.GetMin().y), sprite.palette_id});
        batch.vertices.push_back({pos + size, src.GetMax(), sprite.palette_id});
        batch.vertices.push_back({pos + Vector2(0, size.y), Vector2(src.GetMin().x, src.GetMax().y), sprite.palette_id});
    }
};
```

This covers the complete 2D pixel art rendering system with authentic GBA-style graphics, palette systems, and performance optimizations. The key is maintaining pixel-perfect rendering while providing efficient batching and animation systems.