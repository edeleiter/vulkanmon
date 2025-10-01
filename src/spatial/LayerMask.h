#pragma once

#include <cstdint>

namespace VulkanMon {

// Spatial layer system for filtering entities in spatial queries
// Each bit represents a different type of entity
namespace LayerMask {
    constexpr uint32_t None = 0x00;
    constexpr uint32_t Player = 0x01;
    constexpr uint32_t Creatures = 0x02;
    constexpr uint32_t Terrain = 0x04;
    constexpr uint32_t Grass = 0x08;             // Backward compatibility
    constexpr uint32_t Vegetation = 0x08;        // Generic: tall grass, bushes, cover areas
    constexpr uint32_t Water = 0x10;
    constexpr uint32_t Items = 0x20;
    constexpr uint32_t Pokeballs = 0x40;         // Backward compatibility
    constexpr uint32_t CaptureDevices = 0x40;    // Generic: spheres, nets, capture tools
    constexpr uint32_t Triggers = 0x80;          // Backward compatibility
    constexpr uint32_t TriggerZones = 0x80;      // Generic: encounter zones, events
    constexpr uint32_t NPCs = 0x100;
    constexpr uint32_t Buildings = 0x200;
    constexpr uint32_t Collectibles = 0x400;
    constexpr uint32_t Particles = 0x800;
    constexpr uint32_t UI = 0x1000;
    constexpr uint32_t Debug = 0x2000;
    constexpr uint32_t Camera = 0x4000;
    constexpr uint32_t Environment = 0x8000;     // Generic environment objects
    constexpr uint32_t TriggerZone = 0x10000;    // Singular form for consistency
    constexpr uint32_t All = 0xFFFFFFFF;

    // Commonly used combinations
    constexpr uint32_t Interactables = Items | CaptureDevices | NPCs | Collectibles;
    constexpr uint32_t EnvironmentStatic = Terrain | Vegetation | Water | Buildings;
    constexpr uint32_t Default = Creatures;      // Default layer for new entities
    constexpr uint32_t GameplayEntities = Player | Creatures | Interactables;
    constexpr uint32_t StaticObjects = Terrain | Buildings;
    constexpr uint32_t DynamicObjects = Player | Creatures | Items | CaptureDevices;

    // Utility functions
    inline bool contains(uint32_t mask, uint32_t layer) {
        return (mask & layer) != 0;
    }

    inline uint32_t combine(uint32_t mask1, uint32_t mask2) {
        return mask1 | mask2;
    }

    inline uint32_t remove(uint32_t mask, uint32_t layer) {
        return mask & ~layer;
    }

    inline bool isEmpty(uint32_t mask) {
        return mask == None;
    }

    inline bool isAll(uint32_t mask) {
        return mask == All;
    }

    // Get human-readable string for debugging
    const char* toString(uint32_t layer);
}

} // namespace VulkanMon