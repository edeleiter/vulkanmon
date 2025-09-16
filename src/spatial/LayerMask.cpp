#include "LayerMask.h"

namespace VulkanMon {

const char* LayerMask::toString(uint32_t layer) {
    switch (layer) {
        case None: return "None";
        case Player: return "Player";
        case Creatures: return "Creatures";
        case Terrain: return "Terrain";
        case Grass: return "Grass";
        case Water: return "Water";
        case Items: return "Items";
        case Pokeballs: return "Pokeballs";
        case Triggers: return "Triggers";
        case NPCs: return "NPCs";
        case Buildings: return "Buildings";
        case Collectibles: return "Collectibles";
        case Particles: return "Particles";
        case UI: return "UI";
        case Debug: return "Debug";
        case Camera: return "Camera";
        case All: return "All";
        case Interactables: return "Interactables";
        case Environment: return "Environment";
        case GameplayEntities: return "GameplayEntities";
        case StaticObjects: return "StaticObjects";
        case DynamicObjects: return "DynamicObjects";
        default: return "Unknown";
    }
}

} // namespace VulkanMon