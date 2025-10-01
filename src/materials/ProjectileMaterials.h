#pragma once

#include "../systems/MaterialSystem.h"
#include <unordered_map>
#include <string>

namespace VulkanMon {

/**
 * ProjectileMaterials
 *
 * Curated material presets optimized for projectile rendering.
 * Provides high-contrast, visually distinct materials for different projectile types.
 *
 * Design Philosophy:
 * - Simple is Powerful: Easy-to-use preset system with clear visual distinctions
 * - Engine-Generic: Materials work for bullets, grenades, magic spells, physics objects
 * - Performance-aware: Optimized material properties for fast rendering
 */
class ProjectileMaterials {
public:
    // Projectile material types with distinctive visual properties
    enum class Type {
        // Basic projectile types
        DEFAULT_PROJECTILE,     // Standard gray projectile
        HIGH_VELOCITY_BULLET,   // Bright metallic for fast bullets
        EXPLOSIVE_GRENADE,      // Orange/red for grenades
        MAGIC_SPELL,           // Blue/purple for magical projectiles
        PHYSICS_OBJECT,        // Neutral brown for physics objects

        // Specialized projectile types
        TRACER_BULLET,         // Bright green for tracer rounds
        FIRE_PROJECTILE,       // Red/orange for fire effects
        ICE_PROJECTILE,        // Light blue for ice effects
        POISON_PROJECTILE,     // Green for poison effects
        ENERGY_PROJECTILE,     // Bright cyan for energy weapons

        // Game-specific types
        POKEBALL_RED,          // Classic Pokeball red
        POKEBALL_BLUE,         // Great Ball blue
        POKEBALL_GOLD,         // Ultra Ball gold
        CRYSTAL_PROJECTILE,    // Prismatic crystal effects
        STEALTH_PROJECTILE,    // Dark/transparent for stealth

        CUSTOM                 // User-defined material
    };

    /**
     * Get material data for a specific projectile type
     * @param type Projectile material type
     * @return MaterialData with optimized properties for the projectile type
     */
    static MaterialData getMaterialData(Type type);

    /**
     * Get all available projectile material presets
     * @return Map of material names to types
     */
    static std::unordered_map<std::string, Type> getAllPresets();

    /**
     * Get display name for a material type
     * @param type Material type
     * @return Human-readable name
     */
    static std::string getDisplayName(Type type);

    /**
     * Get recommended physics properties for a material type
     * @param type Material type
     * @return Struct with mass, bounce, drag recommendations
     */
    struct PhysicsProperties {
        float mass;         // Recommended mass in kg
        float bounce;       // Recommended restitution (0-1)
        float drag;         // Recommended drag coefficient
        float scale;        // Recommended scale multiplier
    };

    static PhysicsProperties getPhysicsProperties(Type type);

    /**
     * Create material with engine integration
     * @param materialSystem Material system to register with
     * @param type Projectile material type
     * @return Material ID for use with renderer
     */
    static uint32_t createMaterial(MaterialSystem& materialSystem, Type type);

private:
    // Material data definitions
    static MaterialData createDefaultProjectile();
    static MaterialData createHighVelocityBullet();
    static MaterialData createExplosiveGrenade();
    static MaterialData createMagicSpell();
    static MaterialData createPhysicsObject();
    static MaterialData createTracerBullet();
    static MaterialData createFireProjectile();
    static MaterialData createIceProjectile();
    static MaterialData createPoisonProjectile();
    static MaterialData createEnergyProjectile();
    static MaterialData createPokeballRed();
    static MaterialData createPokeballBlue();
    static MaterialData createPokeballGold();
    static MaterialData createCrystalProjectile();
    static MaterialData createStealthProjectile();
};

/**
 * ProjectileMaterialManager
 *
 * Convenience class for managing projectile materials in the engine.
 * Handles material registration and provides easy access for projectile systems.
 */
class ProjectileMaterialManager {
public:
    ProjectileMaterialManager(MaterialSystem& materialSystem);

    /**
     * Initialize all projectile material presets
     * Call once during engine initialization
     */
    void initializePresets();

    /**
     * Get material ID for a projectile type
     * @param type Projectile material type
     * @return Material ID, or 0 if not found
     */
    uint32_t getMaterialId(ProjectileMaterials::Type type) const;

    /**
     * Get material data for a projectile type
     * @param type Projectile material type
     * @return Material data reference
     */
    const MaterialData& getMaterialData(ProjectileMaterials::Type type) const;

    /**
     * Register a custom material for projectiles
     * @param name Custom material name
     * @param materialData Custom material properties
     * @return Material ID for the custom material
     */
    uint32_t registerCustomMaterial(const std::string& name, const MaterialData& materialData);

    /**
     * Get all registered material types and IDs
     * @return Map of material types to material IDs
     */
    const std::unordered_map<ProjectileMaterials::Type, uint32_t>& getAllMaterials() const;

private:
    MaterialSystem& materialSystem_;
    std::unordered_map<ProjectileMaterials::Type, uint32_t> materialIds_;
    std::unordered_map<std::string, uint32_t> customMaterials_;
};

} // namespace VulkanMon