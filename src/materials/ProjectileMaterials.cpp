#include "ProjectileMaterials.h"
#include "../utils/Logger.h"

namespace VulkanMon {

MaterialData ProjectileMaterials::getMaterialData(Type type) {
    switch (type) {
        case Type::DEFAULT_PROJECTILE:     return createDefaultProjectile();
        case Type::HIGH_VELOCITY_BULLET:   return createHighVelocityBullet();
        case Type::EXPLOSIVE_GRENADE:      return createExplosiveGrenade();
        case Type::MAGIC_SPELL:           return createMagicSpell();
        case Type::PHYSICS_OBJECT:        return createPhysicsObject();
        case Type::TRACER_BULLET:         return createTracerBullet();
        case Type::FIRE_PROJECTILE:       return createFireProjectile();
        case Type::ICE_PROJECTILE:        return createIceProjectile();
        case Type::POISON_PROJECTILE:     return createPoisonProjectile();
        case Type::ENERGY_PROJECTILE:     return createEnergyProjectile();
        case Type::POKEBALL_RED:          return createPokeballRed();
        case Type::POKEBALL_BLUE:         return createPokeballBlue();
        case Type::POKEBALL_GOLD:         return createPokeballGold();
        case Type::CRYSTAL_PROJECTILE:    return createCrystalProjectile();
        case Type::STEALTH_PROJECTILE:    return createStealthProjectile();
        case Type::CUSTOM:                return createDefaultProjectile(); // Fallback
        default:                          return createDefaultProjectile();
    }
}

std::unordered_map<std::string, ProjectileMaterials::Type> ProjectileMaterials::getAllPresets() {
    return {
        {"Default", Type::DEFAULT_PROJECTILE},
        {"Bullet", Type::HIGH_VELOCITY_BULLET},
        {"Grenade", Type::EXPLOSIVE_GRENADE},
        {"Magic", Type::MAGIC_SPELL},
        {"Physics", Type::PHYSICS_OBJECT},
        {"Tracer", Type::TRACER_BULLET},
        {"Fire", Type::FIRE_PROJECTILE},
        {"Ice", Type::ICE_PROJECTILE},
        {"Poison", Type::POISON_PROJECTILE},
        {"Energy", Type::ENERGY_PROJECTILE},
        {"Pokeball Red", Type::POKEBALL_RED},
        {"Pokeball Blue", Type::POKEBALL_BLUE},
        {"Pokeball Gold", Type::POKEBALL_GOLD},
        {"Crystal", Type::CRYSTAL_PROJECTILE},
        {"Stealth", Type::STEALTH_PROJECTILE}
    };
}

std::string ProjectileMaterials::getDisplayName(Type type) {
    switch (type) {
        case Type::DEFAULT_PROJECTILE:     return "Default Projectile";
        case Type::HIGH_VELOCITY_BULLET:   return "High Velocity Bullet";
        case Type::EXPLOSIVE_GRENADE:      return "Explosive Grenade";
        case Type::MAGIC_SPELL:           return "Magic Spell";
        case Type::PHYSICS_OBJECT:        return "Physics Object";
        case Type::TRACER_BULLET:         return "Tracer Bullet";
        case Type::FIRE_PROJECTILE:       return "Fire Projectile";
        case Type::ICE_PROJECTILE:        return "Ice Projectile";
        case Type::POISON_PROJECTILE:     return "Poison Projectile";
        case Type::ENERGY_PROJECTILE:     return "Energy Projectile";
        case Type::POKEBALL_RED:          return "Pokeball (Red)";
        case Type::POKEBALL_BLUE:         return "Pokeball (Blue)";
        case Type::POKEBALL_GOLD:         return "Pokeball (Gold)";
        case Type::CRYSTAL_PROJECTILE:    return "Crystal Projectile";
        case Type::STEALTH_PROJECTILE:    return "Stealth Projectile";
        case Type::CUSTOM:                return "Custom Material";
        default:                          return "Unknown";
    }
}

ProjectileMaterials::PhysicsProperties ProjectileMaterials::getPhysicsProperties(Type type) {
    switch (type) {
        case Type::DEFAULT_PROJECTILE:
            return {0.1f, 0.3f, 0.01f, 1.0f};    // Balanced properties

        case Type::HIGH_VELOCITY_BULLET:
            return {0.01f, 0.1f, 0.001f, 0.5f};  // Light, fast, low drag

        case Type::EXPLOSIVE_GRENADE:
            return {0.5f, 0.4f, 0.02f, 1.2f};    // Heavy, bouncy

        case Type::MAGIC_SPELL:
            return {0.05f, 0.0f, 0.0f, 1.0f};    // Light, no physics resistance

        case Type::PHYSICS_OBJECT:
            return {1.0f, 0.6f, 0.015f, 1.0f};   // Realistic physics object

        case Type::TRACER_BULLET:
            return {0.02f, 0.1f, 0.002f, 0.7f};  // Slightly heavier than bullet

        case Type::FIRE_PROJECTILE:
            return {0.03f, 0.1f, 0.005f, 1.0f};  // Light with some drag

        case Type::ICE_PROJECTILE:
            return {0.15f, 0.2f, 0.01f, 1.0f};   // Dense ice properties

        case Type::POISON_PROJECTILE:
            return {0.08f, 0.3f, 0.012f, 1.0f};  // Liquid-like properties

        case Type::ENERGY_PROJECTILE:
            return {0.01f, 0.0f, 0.0f, 1.0f};    // Nearly massless energy

        case Type::POKEBALL_RED:
        case Type::POKEBALL_BLUE:
        case Type::POKEBALL_GOLD:
            return {0.2f, 0.4f, 0.015f, 1.0f};   // Pokeball weight and bounce

        case Type::CRYSTAL_PROJECTILE:
            return {0.3f, 0.8f, 0.01f, 1.0f};    // Heavy crystal, very bouncy

        case Type::STEALTH_PROJECTILE:
            return {0.05f, 0.1f, 0.008f, 0.8f};  // Light, minimal physics

        default:
            return {0.1f, 0.3f, 0.01f, 1.0f};    // Default fallback
    }
}

uint32_t ProjectileMaterials::createMaterial(MaterialSystem& materialSystem, Type type) {
    MaterialData data = getMaterialData(type);
    return materialSystem.createMaterial(data);
}

// Private material creation methods

MaterialData ProjectileMaterials::createDefaultProjectile() {
    // Standard gray projectile - neutral and visible
    return MaterialData(
        glm::vec3(0.2f, 0.2f, 0.2f),    // Dark gray ambient
        glm::vec3(0.6f, 0.6f, 0.6f),    // Medium gray diffuse
        glm::vec3(0.8f, 0.8f, 0.8f),    // Light gray specular
        32.0f                           // Medium shininess
    );
}

MaterialData ProjectileMaterials::createHighVelocityBullet() {
    // Bright metallic for high-speed visibility
    return MaterialData(
        glm::vec3(0.3f, 0.3f, 0.1f),    // Brass-like ambient
        glm::vec3(0.9f, 0.8f, 0.4f),    // Bright metallic diffuse
        glm::vec3(1.0f, 1.0f, 0.8f),    // Bright specular
        128.0f                          // Very shiny
    );
}

MaterialData ProjectileMaterials::createExplosiveGrenade() {
    // Orange/red for danger indication
    return MaterialData(
        glm::vec3(0.3f, 0.1f, 0.0f),    // Dark red ambient
        glm::vec3(0.9f, 0.4f, 0.1f),    // Bright orange diffuse
        glm::vec3(1.0f, 0.6f, 0.3f),    // Orange-yellow specular
        64.0f                           // Moderate shininess
    );
}

MaterialData ProjectileMaterials::createMagicSpell() {
    // Blue/purple for magical effects
    return MaterialData(
        glm::vec3(0.1f, 0.0f, 0.3f),    // Dark purple ambient
        glm::vec3(0.4f, 0.2f, 0.9f),    // Bright purple diffuse
        glm::vec3(0.8f, 0.6f, 1.0f),    // Light purple specular
        256.0f                          // Very magical shine
    );
}

MaterialData ProjectileMaterials::createPhysicsObject() {
    // Neutral brown for realistic physics objects
    return MaterialData(
        glm::vec3(0.2f, 0.15f, 0.1f),   // Dark brown ambient
        glm::vec3(0.6f, 0.4f, 0.2f),    // Medium brown diffuse
        glm::vec3(0.4f, 0.3f, 0.2f),    // Muted brown specular
        16.0f                           // Low shininess
    );
}

MaterialData ProjectileMaterials::createTracerBullet() {
    // Bright green for visibility
    return MaterialData(
        glm::vec3(0.0f, 0.3f, 0.0f),    // Dark green ambient
        glm::vec3(0.2f, 1.0f, 0.2f),    // Bright green diffuse
        glm::vec3(0.4f, 1.0f, 0.4f),    // Very bright green specular
        512.0f                          // Maximum shine for visibility
    );
}

MaterialData ProjectileMaterials::createFireProjectile() {
    // Red/orange fire colors
    return MaterialData(
        glm::vec3(0.4f, 0.2f, 0.0f),    // Dark orange ambient
        glm::vec3(1.0f, 0.4f, 0.0f),    // Bright orange diffuse
        glm::vec3(1.0f, 0.6f, 0.2f),    // Yellow-orange specular
        128.0f                          // Bright shine
    );
}

MaterialData ProjectileMaterials::createIceProjectile() {
    // Light blue ice colors
    return MaterialData(
        glm::vec3(0.1f, 0.2f, 0.3f),    // Cool blue ambient
        glm::vec3(0.4f, 0.7f, 1.0f),    // Light blue diffuse
        glm::vec3(0.8f, 0.9f, 1.0f),    // Very light blue specular
        256.0f                          // Icy shine
    );
}

MaterialData ProjectileMaterials::createPoisonProjectile() {
    // Sickly green poison colors
    return MaterialData(
        glm::vec3(0.2f, 0.3f, 0.1f),    // Dark green ambient
        glm::vec3(0.4f, 0.8f, 0.2f),    // Bright sickly green diffuse
        glm::vec3(0.6f, 1.0f, 0.4f),    // Toxic green specular
        64.0f                           // Moderate shine
    );
}

MaterialData ProjectileMaterials::createEnergyProjectile() {
    // Bright cyan energy colors
    return MaterialData(
        glm::vec3(0.0f, 0.3f, 0.3f),    // Dark cyan ambient
        glm::vec3(0.0f, 1.0f, 1.0f),    // Bright cyan diffuse
        glm::vec3(0.4f, 1.0f, 1.0f),    // Very bright cyan specular
        512.0f                          // Maximum energy shine
    );
}

MaterialData ProjectileMaterials::createPokeballRed() {
    // Classic Pokeball red and white
    return MaterialData(
        glm::vec3(0.3f, 0.0f, 0.0f),    // Dark red ambient
        glm::vec3(0.9f, 0.1f, 0.1f),    // Bright red diffuse
        glm::vec3(1.0f, 0.4f, 0.4f),    // Light red specular
        64.0f                           // Pokeball shine
    );
}

MaterialData ProjectileMaterials::createPokeballBlue() {
    // Great Ball blue colors
    return MaterialData(
        glm::vec3(0.0f, 0.1f, 0.3f),    // Dark blue ambient
        glm::vec3(0.1f, 0.4f, 0.9f),    // Bright blue diffuse
        glm::vec3(0.4f, 0.6f, 1.0f),    // Light blue specular
        64.0f                           // Pokeball shine
    );
}

MaterialData ProjectileMaterials::createPokeballGold() {
    // Ultra Ball gold colors
    return MaterialData(
        glm::vec3(0.3f, 0.2f, 0.0f),    // Dark gold ambient
        glm::vec3(1.0f, 0.8f, 0.2f),    // Bright gold diffuse
        glm::vec3(1.0f, 1.0f, 0.6f),    // Very bright gold specular
        128.0f                          // Shiny gold
    );
}

MaterialData ProjectileMaterials::createCrystalProjectile() {
    // Prismatic crystal colors
    return MaterialData(
        glm::vec3(0.2f, 0.2f, 0.2f),    // Clear ambient
        glm::vec3(0.8f, 0.8f, 0.9f),    // Clear diffuse with slight blue
        glm::vec3(1.0f, 1.0f, 1.0f),    // Perfect white specular
        1024.0f                         // Maximum crystal shine
    );
}

MaterialData ProjectileMaterials::createStealthProjectile() {
    // Dark colors for stealth
    return MaterialData(
        glm::vec3(0.05f, 0.05f, 0.05f), // Very dark ambient
        glm::vec3(0.1f, 0.1f, 0.15f),   // Dark blue-gray diffuse
        glm::vec3(0.2f, 0.2f, 0.25f),   // Muted specular
        8.0f                            // Very low shine
    );
}

// ProjectileMaterialManager implementation

ProjectileMaterialManager::ProjectileMaterialManager(MaterialSystem& materialSystem)
    : materialSystem_(materialSystem) {
}

void ProjectileMaterialManager::initializePresets() {
    VKMON_INFO("ProjectileMaterialManager: Initializing projectile material presets...");

    // Create all preset materials
    auto presets = ProjectileMaterials::getAllPresets();
    for (const auto& [name, type] : presets) {
        if (type != ProjectileMaterials::Type::CUSTOM) {
            uint32_t materialId = ProjectileMaterials::createMaterial(materialSystem_, type);
            materialIds_[type] = materialId;
            VKMON_DEBUG("  Created material: " + name + " (ID: " + std::to_string(materialId) + ")");
        }
    }

    VKMON_INFO("ProjectileMaterialManager: Initialized " + std::to_string(materialIds_.size()) + " preset materials");
}

uint32_t ProjectileMaterialManager::getMaterialId(ProjectileMaterials::Type type) const {
    auto it = materialIds_.find(type);
    if (it != materialIds_.end()) {
        return it->second;
    }

    VKMON_WARNING("ProjectileMaterialManager: Material type not found, returning default (0)");
    return 0; // Default material ID
}

const MaterialData& ProjectileMaterialManager::getMaterialData(ProjectileMaterials::Type type) const {
    uint32_t materialId = getMaterialId(type);
    return materialSystem_.getMaterialData(materialId);
}

uint32_t ProjectileMaterialManager::registerCustomMaterial(const std::string& name, const MaterialData& materialData) {
    uint32_t materialId = materialSystem_.createMaterial(materialData);
    customMaterials_[name] = materialId;

    VKMON_INFO("ProjectileMaterialManager: Registered custom material '" + name + "' (ID: " + std::to_string(materialId) + ")");
    return materialId;
}

const std::unordered_map<ProjectileMaterials::Type, uint32_t>& ProjectileMaterialManager::getAllMaterials() const {
    return materialIds_;
}

} // namespace VulkanMon