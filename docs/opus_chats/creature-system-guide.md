# Creature System Foundation
## Week 4: Building a Pokemon-Style Creature Framework

## Overview
The creature system is the heart of your Pokemon-like game. We need flexible data structures for stats, moves, types, and behaviors, all integrated with your ECS architecture. This guide covers everything from basic stats to AI behavior trees.

## Core Architecture

### Creature Data Model

```cpp
// include/creature/CreatureData.h
#pragma once
#include <string>
#include <vector>
#include <array>

namespace VulkanMon {

// Type effectiveness system
enum class CreatureType {
    Normal, Fire, Water, Grass, Electric, Ice,
    Fighting, Poison, Ground, Flying, Psychic, Bug,
    Rock, Ghost, Dragon, Dark, Steel, Fairy,
    COUNT
};

// Base stats (like Pokemon's base stats)
struct BaseStats {
    uint16_t hp = 50;
    uint16_t attack = 50;
    uint16_t defense = 50;
    uint16_t spAttack = 50;
    uint16_t spDefense = 50;
    uint16_t speed = 50;
    
    // Calculate stat at given level with IVs and EVs
    uint16_t calculateHP(uint8_t level, uint8_t iv, uint16_t ev) const {
        return ((2 * hp + iv + ev/4) * level / 100) + level + 10;
    }
    
    uint16_t calculateStat(uint16_t base, uint8_t level, uint8_t iv, uint16_t ev, float nature = 1.0f) const {
        return (((2 * base + iv + ev/4) * level / 100) + 5) * nature;
    }
};

// Individual values (genetics)
struct IVs {
    uint8_t hp : 5;      // 0-31
    uint8_t attack : 5;
    uint8_t defense : 5;
    uint8_t spAttack : 5;
    uint8_t spDefense : 5;
    uint8_t speed : 5;
    
    static IVs random() {
        IVs ivs;
        ivs.hp = rand() % 32;
        ivs.attack = rand() % 32;
        ivs.defense = rand() % 32;
        ivs.spAttack = rand() % 32;
        ivs.spDefense = rand() % 32;
        ivs.speed = rand() % 32;
        return ivs;
    }
};

// Effort values (training)
struct EVs {
    uint16_t hp = 0;      // 0-252
    uint16_t attack = 0;
    uint16_t defense = 0;
    uint16_t spAttack = 0;
    uint16_t spDefense = 0;
    uint16_t speed = 0;
    
    uint16_t total() const {
        return hp + attack + defense + spAttack + spDefense + speed;
    }
    
    bool canAdd(uint16_t amount) const {
        return total() + amount <= 510;  // Max total EVs
    }
};

// Nature modifies stats
enum class Nature {
    Hardy, Lonely, Brave, Adamant, Naughty,
    Bold, Docile, Relaxed, Impish, Lax,
    Timid, Hasty, Serious, Jolly, Naive,
    Modest, Mild, Quiet, Bashful, Rash,
    Calm, Gentle, Sassy, Careful, Quirky,
    COUNT
};

struct NatureModifiers {
    float attack = 1.0f;
    float defense = 1.0f;
    float spAttack = 1.0f;
    float spDefense = 1.0f;
    float speed = 1.0f;
};

// Move data
struct Move {
    uint16_t id;
    std::string name;
    CreatureType type;
    uint8_t power;        // 0-255
    uint8_t accuracy;     // 0-100
    uint8_t pp;          // Power points
    uint8_t priority;    // -7 to +5
    
    enum Category {
        Physical, Special, Status
    } category;
    
    enum Target {
        Single, AllOpponents, AllAllies, Self, Field
    } target;
    
    // Secondary effects
    struct Effect {
        enum Type {
            None, Burn, Freeze, Paralysis, Poison, Sleep,
            Confusion, Flinch, StatChange
        } type = None;
        
        int8_t statChange = 0;  // -6 to +6 stages
        uint8_t chance = 100;    // Percent chance
    } effect;
};

// Species definition (like Pokedex data)
struct CreatureSpecies {
    uint16_t id;
    std::string name;
    std::string category;  // e.g., "Mouse Pokemon"
    
    CreatureType primaryType;
    CreatureType secondaryType = CreatureType::COUNT;  // COUNT = no secondary type
    
    BaseStats baseStats;
    
    // Appearance
    std::string modelPath;
    std::string texturePath;
    std::string shinyTexturePath;
    float height = 1.0f;  // meters
    float weight = 10.0f; // kg
    
    // Catch rate and experience
    uint8_t catchRate = 45;  // 3-255, lower = harder
    uint8_t baseExperience = 64;
    
    enum GrowthRate {
        Erratic, Fast, MediumFast, MediumSlow, Slow, Fluctuating
    } growthRate = MediumFast;
    
    // Abilities
    std::vector<uint16_t> possibleAbilities;
    uint16_t hiddenAbility = 0;
    
    // Moves
    std::vector<std::pair<uint8_t, uint16_t>> levelUpMoves;  // level, moveId
    std::vector<uint16_t> tmMoves;  // Technical machine moves
    std::vector<uint16_t> eggMoves;
    
    // Evolution
    struct Evolution {
        uint16_t toSpecies;
        enum Method {
            Level, Item, Trade, Friendship, Time, Location, Move
        } method;
        uint16_t requirement;  // Level, item ID, etc.
    };
    std::vector<Evolution> evolutions;
    
    // Breeding
    std::vector<uint16_t> eggGroups;
    uint16_t hatchSteps = 5120;
    float maleRatio = 0.5f;  // 0 = all female, 1 = all male, -1 = genderless
};

} // namespace VulkanMon
```

### Creature Component

```cpp
// include/creature/CreatureComponent.h
#pragma once
#include "ecs/Component.h"
#include "creature/CreatureData.h"
#include <array>

namespace VulkanMon {

class CreatureComponent : public Component {
public:
    // Identity
    uint16_t speciesId;
    std::string nickname;
    uint32_t trainerId;
    uint32_t personalityValue;  // Determines shininess, gender, etc.
    
    // Stats
    uint8_t level = 1;
    uint32_t experience = 0;
    IVs ivs;
    EVs evs;
    Nature nature;
    
    // Current battle stats
    struct BattleStats {
        uint16_t hp;
        uint16_t maxHp;
        uint16_t attack;
        uint16_t defense;
        uint16_t spAttack;
        uint16_t spDefense;
        uint16_t speed;
        
        // Stat stages (-6 to +6)
        int8_t attackStage = 0;
        int8_t defenseStage = 0;
        int8_t spAttackStage = 0;
        int8_t spDefenseStage = 0;
        int8_t speedStage = 0;
        int8_t accuracyStage = 0;
        int8_t evasionStage = 0;
    } battleStats;
    
    // Moves (up to 4)
    struct LearnedMove {
        uint16_t moveId;
        uint8_t currentPP;
        uint8_t maxPP;
    };
    std::array<LearnedMove, 4> moves;
    uint8_t moveCount = 0;
    
    // Status
    enum StatusCondition {
        None, Burn, Freeze, Paralysis, Poison, BadPoison, Sleep
    } status = None;
    uint8_t statusCounter = 0;  // Sleep turns, poison damage, etc.
    
    // Ability
    uint16_t abilityId;
    bool hasHiddenAbility = false;
    
    // Held item
    uint16_t heldItemId = 0;
    
    // Friendship/happiness (0-255)
    uint8_t friendship = 70;
    
    // Contest stats (if you want contests like in Pokemon)
    struct ContestStats {
        uint8_t cool = 0;
        uint8_t beauty = 0;
        uint8_t cute = 0;
        uint8_t smart = 0;
        uint8_t tough = 0;
    } contestStats;
    
    // Methods
    void recalculateStats();
    bool canEvolve() const;
    void addExperience(uint32_t exp);
    bool tryLearnMove(uint16_t moveId);
    float getTypeEffectiveness(CreatureType attackType) const;
    bool isShiny() const {
        return (personalityValue % 8192) == 0;  // 1/8192 chance
    }
};

// Creature AI Component
class CreatureAIComponent : public Component {
public:
    enum AIType {
        Wild,      // Wild Pokemon behavior
        Trainer,   // Owned by NPC trainer
        Partner,   // Player's Pokemon following them
        Boss,      // Special boss AI
        Passive,   // Won't attack unless provoked
        Aggressive // Actively hunts player
    } aiType = Wild;
    
    enum BehaviorState {
        Idle,
        Wander,
        Alert,      // Noticed something
        Approach,   // Moving toward target
        Flee,       // Running away
        Attack,     // In combat
        Follow,     // Following trainer
        Sleep,      // Sleeping (time-based)
        Eat,        // Eating berries, etc.
        Play        // Playing with other creatures
    } currentState = Idle;
    
    // Personality traits (affects behavior)
    struct Personality {
        float aggression = 0.5f;   // 0 = peaceful, 1 = aggressive
        float curiosity = 0.5f;    // Likelihood to investigate
        float playfulness = 0.5f;  // Social interaction
        float fearfulness = 0.5f;  // Likelihood to flee
        float loyalty = 0.5f;      // For owned Pokemon
    } personality;
    
    // Perception
    float sightRange = 15.0f;
    float hearingRange = 20.0f;
    float alertRadius = 5.0f;  // Distance at which creature becomes alert
    
    // Current target/interest
    EntityId targetEntity = InvalidEntity;
    glm::vec3 targetPosition;
    float targetDistance = 0.0f;
    
    // Timers
    float stateTimer = 0.0f;
    float lastStateChange = 0.0f;
    float nextDecisionTime = 0.0f;
    
    // Memory (simple)
    std::vector<EntityId> recentThreats;
    std::vector<EntityId> friendlyEntities;
    glm::vec3 homePosition;  // Where it spawned
    float homeRadius = 30.0f;  // Won't wander beyond this
};

} // namespace VulkanMon
```

### Creature Behavior System

```cpp
// include/creature/CreatureBehaviorSystem.h
#pragma once
#include "ecs/System.h"
#include "creature/CreatureComponent.h"

namespace VulkanMon {

class CreatureBehaviorSystem : public System {
public:
    void update(float deltaTime) override;
    
private:
    // State machines for each AI type
    void updateWildBehavior(EntityId entity, CreatureAIComponent* ai, float deltaTime);
    void updateTrainerPokemon(EntityId entity, CreatureAIComponent* ai, float deltaTime);
    void updatePartnerPokemon(EntityId entity, CreatureAIComponent* ai, float deltaTime);
    
    // Behavior tree nodes
    bool checkThreats(EntityId entity, CreatureAIComponent* ai);
    bool checkHunger(EntityId entity, CreatureAIComponent* ai);
    bool checkSocial(EntityId entity, CreatureAIComponent* ai);
    
    // Actions
    void startWander(EntityId entity, CreatureAIComponent* ai);
    void startFlee(EntityId entity, CreatureAIComponent* ai, EntityId threat);
    void startApproach(EntityId entity, CreatureAIComponent* ai, EntityId target);
    void startAttack(EntityId entity, CreatureAIComponent* ai, EntityId target);
    
    // Utility functions
    float calculateThreatLevel(EntityId creature, EntityId other);
    bool canSee(EntityId observer, EntityId target);
    bool canHear(EntityId listener, EntityId source);
    glm::vec3 getFleeDirection(const glm::vec3& creaturePos, const glm::vec3& threatPos);
};

// Implementation
void CreatureBehaviorSystem::update(float deltaTime) {
    auto& ecs = ECS::getInstance();
    
    // Process all creatures with AI
    for (auto& [entity, ai] : ecs.getComponents<CreatureAIComponent>()) {
        // Update timers
        ai->stateTimer += deltaTime;
        
        // Decision making
        if (getCurrentTime() >= ai->nextDecisionTime) {
            switch (ai->aiType) {
                case CreatureAIComponent::Wild:
                    updateWildBehavior(entity, ai, deltaTime);
                    break;
                case CreatureAIComponent::Partner:
                    updatePartnerPokemon(entity, ai, deltaTime);
                    break;
                // ... other AI types
            }
            
            // Schedule next decision
            ai->nextDecisionTime = getCurrentTime() + 
                randomFloat(0.5f, 2.0f);  // Vary decision frequency
        }
        
        // Execute current behavior
        executeCurrentState(entity, ai, deltaTime);
    }
}

void CreatureBehaviorSystem::updateWildBehavior(EntityId entity, 
                                                CreatureAIComponent* ai, 
                                                float deltaTime) {
    // Behavior tree for wild Pokemon
    
    // Priority 1: Check for threats
    if (checkThreats(entity, ai)) {
        auto* creature = ecs.getComponent<CreatureComponent>(entity);
        float threatLevel = calculateThreatLevel(entity, ai->targetEntity);
        
        // Fight or flight decision based on personality and threat
        if (threatLevel > ai->personality.fearfulness * 2.0f) {
            startFlee(entity, ai, ai->targetEntity);
        } else if (ai->personality.aggression > 0.7f) {
            startAttack(entity, ai, ai->targetEntity);
        } else {
            // Alert but don't engage
            ai->currentState = CreatureAIComponent::Alert;
        }
        return;
    }
    
    // Priority 2: Check for food/water
    if (checkHunger(entity, ai)) {
        // Look for berries or water sources
        auto foodSources = findNearbyFood(entity);
        if (!foodSources.empty()) {
            startApproach(entity, ai, foodSources[0]);
            ai->currentState = CreatureAIComponent::Eat;
        }
        return;
    }
    
    // Priority 3: Social interaction
    if (ai->personality.playfulness > 0.6f && checkSocial(entity, ai)) {
        // Find other creatures of same species
        auto friends = findSameSpecies(entity, 10.0f);
        if (!friends.empty() && randomFloat() < ai->personality.playfulness) {
            startApproach(entity, ai, friends[0]);
            ai->currentState = CreatureAIComponent::Play;
        }
        return;
    }
    
    // Priority 4: Exploration/Wandering
    if (ai->currentState == CreatureAIComponent::Idle) {
        if (randomFloat() < ai->personality.curiosity * deltaTime) {
            startWander(entity, ai);
        }
    }
    
    // Check if too far from home
    auto* transform = ecs.getComponent<Transform>(entity);
    float distFromHome = glm::distance(transform->position, ai->homePosition);
    if (distFromHome > ai->homeRadius) {
        // Head back home
        ai->targetPosition = ai->homePosition;
        ai->currentState = CreatureAIComponent::Approach;
    }
}

bool CreatureBehaviorSystem::checkThreats(EntityId entity, CreatureAIComponent* ai) {
    auto& spatial = getSpatialSystem();
    
    // Query for nearby entities
    auto nearby = spatial.queryRadius(
        getPosition(entity),
        ai->sightRange,
        LayerMask::Player | LayerMask::Creatures
    );
    
    for (const auto& result : nearby) {
        if (result.entity == entity) continue;
        
        // Check if this entity is a threat
        bool isThreat = false;
        
        // Players are threats if creature is aggressive or player is too close
        if (hasComponent<PlayerComponent>(result.entity)) {
            isThreat = (result.distance < ai->alertRadius) || 
                      (ai->personality.aggression > 0.8f);
        }
        
        // Other creatures might be threats (predator/prey relationships)
        if (hasComponent<CreatureComponent>(result.entity)) {
            auto* other = getComponent<CreatureComponent>(result.entity);
            isThreat = isNaturalPredator(other->speciesId, 
                                         getComponent<CreatureComponent>(entity)->speciesId);
        }
        
        if (isThreat && canSee(entity, result.entity)) {
            ai->targetEntity = result.entity;
            ai->targetDistance = result.distance;
            return true;
        }
    }
    
    return false;
}

void CreatureBehaviorSystem::startWander(EntityId entity, CreatureAIComponent* ai) {
    // Pick random destination within home radius
    float angle = randomFloat(0, TWO_PI);
    float distance = randomFloat(5.0f, 15.0f);
    
    glm::vec3 offset(
        cos(angle) * distance,
        0,
        sin(angle) * distance
    );
    
    ai->targetPosition = ai->homePosition + offset;
    ai->currentState = CreatureAIComponent::Wander;
    ai->stateTimer = 0.0f;
    
    // Set movement speed based on state
    auto* movement = getComponent<MovementComponent>(entity);
    if (movement) {
        movement->moveSpeed = 2.0f;  // Casual walking speed
    }
}

} // namespace VulkanMon
```

### Creature Spawning System

```cpp
// include/creature/CreatureSpawner.h
#pragma once
#include "ecs/System.h"
#include "creature/CreatureData.h"

namespace VulkanMon {

// Spawn rules for different areas
struct SpawnRule {
    uint16_t speciesId;
    float weight;  // Relative spawn chance
    uint8_t minLevel;
    uint8_t maxLevel;
    
    enum TimeOfDay {
        Any, Morning, Day, Evening, Night
    } timeRequirement = Any;
    
    enum Weather {
        AnyWeather, Clear, Rain, Snow, Fog, Storm
    } weatherRequirement = AnyWeather;
};

struct SpawnZone {
    glm::vec3 center;
    float radius;
    std::vector<SpawnRule> spawnRules;
    
    uint8_t maxCreatures = 10;
    uint8_t currentCreatures = 0;
    float spawnRate = 0.1f;  // Spawns per second
    float lastSpawnTime = 0.0f;
};

class CreatureSpawner : public System {
public:
    void initialize() override;
    void update(float deltaTime) override;
    
    // Zone management
    void addSpawnZone(const SpawnZone& zone);
    void removeSpawnZone(size_t zoneId);
    
    // Manual spawning
    EntityId spawnCreature(uint16_t speciesId, 
                          const glm::vec3& position,
                          uint8_t level);
    
    EntityId spawnWildCreature(const glm::vec3& position,
                               const SpawnZone& zone);
    
private:
    std::vector<SpawnZone> m_spawnZones;
    std::unordered_map<uint16_t, CreatureSpecies> m_speciesDatabase;
    
    // Spawning logic
    uint16_t selectSpecies(const SpawnZone& zone);
    uint8_t selectLevel(const SpawnRule& rule);
    void initializeCreature(EntityId entity, uint16_t speciesId, uint8_t level);
    
    // Cleanup
    void despawnDistantCreatures();
    void respawnCulledCreatures();
};

// Implementation
EntityId CreatureSpawner::spawnCreature(uint16_t speciesId,
                                        const glm::vec3& position,
                                        uint8_t level) {
    auto& ecs = ECS::getInstance();
    EntityId creature = ecs.createEntity();
    
    // Core components
    auto* transform = ecs.addComponent<Transform>(creature);
    transform->position = position;
    
    // Creature data
    auto* creatureComp = ecs.addComponent<CreatureComponent>(creature);
    creatureComp->speciesId = speciesId;
    creatureComp->level = level;
    creatureComp->ivs = IVs::random();
    creatureComp->nature = static_cast<Nature>(rand() % static_cast<int>(Nature::COUNT));
    creatureComp->personalityValue = rand();
    
    // Load species data
    const auto& species = m_speciesDatabase[speciesId];
    
    // Calculate stats
    creatureComp->recalculateStats();
    creatureComp->battleStats.hp = creatureComp->battleStats.maxHp;
    
    // Add model/rendering
    auto* renderable = ecs.addComponent<RenderableComponent>(creature);
    renderable->modelPath = species.modelPath;
    renderable->texturePath = creatureComp->isShiny() ? 
        species.shinyTexturePath : species.texturePath;
    transform->scale = glm::vec3(species.height);
    
    // Physics
    auto* physics = ecs.addComponent<PhysicsComponent>(creature);
    physics->bodyType = BodyType::Kinematic;
    physics->shapeType = PhysicsComponent::Capsule;
    physics->shapeSize = glm::vec3(species.height * 0.3f, species.height, 0);
    physics->collisionGroup = CollisionGroup::Creature;
    
    // Spatial component for queries
    auto* spatial = ecs.addComponent<SpatialComponent>(creature);
    spatial->position = position;
    spatial->radius = species.height;
    spatial->spatialLayers = LayerMask::Creatures;
    
    // AI for wild creatures
    auto* ai = ecs.addComponent<CreatureAIComponent>(creature);
    ai->aiType = CreatureAIComponent::Wild;
    ai->homePosition = position;
    
    // Randomize personality
    ai->personality.aggression = randomFloat(0.2f, 0.8f);
    ai->personality.curiosity = randomFloat(0.3f, 0.9f);
    ai->personality.playfulness = randomFloat(0.1f, 0.7f);
    ai->personality.fearfulness = randomFloat(0.3f, 0.8f);
    
    // Animation
    auto* animation = ecs.addComponent<AnimationComponent>(creature);
    animation->addAnimation("idle", "animations/" + species.name + "_idle.anim");
    animation->addAnimation("walk", "animations/" + species.name + "_walk.anim");
    animation->addAnimation("run", "animations/" + species.name + "_run.anim");
    animation->addAnimation("attack", "animations/" + species.name + "_attack.anim");
    animation->playAnimation("idle", true);
    
    // Learn starting moves
    for (const auto& [moveLevel, moveId] : species.levelUpMoves) {
        if (moveLevel <= level) {
            creatureComp->tryLearnMove(moveId);
        }
    }
    
    return creature;
}

void CreatureSpawner::update(float deltaTime) {
    float currentTime = getCurrentTime();
    
    for (auto& zone : m_spawnZones) {
        // Check if we should spawn
        if (zone.currentCreatures < zone.maxCreatures &&
            currentTime - zone.lastSpawnTime > (1.0f / zone.spawnRate)) {
            
            // Check if player is nearby (don't spawn in view)
            auto playerPos = getPlayerPosition();
            float distToPlayer = glm::distance(playerPos, zone.center);
            
            if (distToPlayer > 20.0f && distToPlayer < 100.0f) {
                // Find spawn position
                glm::vec3 spawnPos = findValidSpawnPosition(zone);
                
                if (spawnPos != glm::vec3(0)) {
                    EntityId creature = spawnWildCreature(spawnPos, zone);
                    zone.currentCreatures++;
                    zone.lastSpawnTime = currentTime;
                }
            }
        }
    }
    
    // Cleanup distant creatures
    despawnDistantCreatures();
}

} // namespace VulkanMon
```

### Battle System Integration

```cpp
// include/creature/BattleSystem.h
#pragma once
#include "creature/CreatureComponent.h"

namespace VulkanMon {

class BattleSystem : public System {
public:
    struct BattleState {
        enum Phase {
            NotInBattle,
            Starting,
            SelectingAction,
            ExecutingTurn,
            EndingTurn,
            Victory,
            Defeat,
            Fled,
            Caught
        } phase = NotInBattle;
        
        // Participants
        std::vector<EntityId> playerTeam;
        std::vector<EntityId> opponentTeam;
        
        EntityId activePlayerCreature = InvalidEntity;
        EntityId activeOpponentCreature = InvalidEntity;
        
        // Turn order (calculated each turn based on speed)
        std::vector<EntityId> turnOrder;
        size_t currentTurnIndex = 0;
        
        // Selected actions
        struct Action {
            enum Type {
                None, Move, Switch, Item, Flee
            } type = None;
            
            uint16_t moveId = 0;
            EntityId targetEntity = InvalidEntity;
            uint16_t itemId = 0;
            EntityId switchTo = InvalidEntity;
        };
        
        std::unordered_map<EntityId, Action> selectedActions;
    };
    
    // Battle management
    void startBattle(const std::vector<EntityId>& playerTeam,
                    const std::vector<EntityId>& opponentTeam);
    void endBattle(BattleState::Phase result);
    
    // Turn execution
    void selectAction(EntityId creature, const BattleState::Action& action);
    void executeTurn();
    
    // Move execution
    void executeMove(EntityId attacker, EntityId defender, uint16_t moveId);
    float calculateDamage(const CreatureComponent* attacker,
                         const CreatureComponent* defender,
                         const Move& move);
    
private:
    BattleState m_battleState;
    
    // Damage calculation
    float getTypeEffectiveness(CreatureType attackType, 
                               CreatureType defenseType);
    float getSTAB(const CreatureComponent* attacker, CreatureType moveType);
    bool checkCritical(const CreatureComponent* attacker);
    float randomDamageRoll() { return randomFloat(0.85f, 1.0f); }
};

} // namespace VulkanMon
```

## Testing Suite

```cpp
// tests_cpp/creature/test_CreatureSystem.cpp
TEST_CASE("Creature Stats Calculation", "[creature]") {
    CreatureComponent creature;
    creature.speciesId = 1;  // Bulbasaur equivalent
    creature.level = 50;
    
    // Set base stats
    BaseStats base;
    base.hp = 45;
    base.attack = 49;
    base.defense = 49;
    base.spAttack = 65;
    base.spDefense = 65;
    base.speed = 45;
    
    // Perfect IVs
    creature.ivs.hp = 31;
    creature.ivs.attack = 31;
    
    // Some EVs
    creature.evs.hp = 252;
    creature.evs.attack = 252;
    
    creature.recalculateStats();
    
    CHECK(creature.battleStats.maxHp == 175);  // Expected HP
    CHECK(creature.battleStats.attack > 100);  // With EVs and IVs
}

TEST_CASE("Type Effectiveness", "[creature]") {
    CreatureComponent attacker, defender;
    
    SECTION("Super effective") {
        float effectiveness = getTypeEffectiveness(
            CreatureType::Fire, 
            CreatureType::Grass
        );
        CHECK(effectiveness == 2.0f);
    }
    
    SECTION("Not very effective") {
        float effectiveness = getTypeEffectiveness(
            CreatureType::Fire,
            CreatureType::Water
        );
        CHECK(effectiveness == 0.5f);
    }
    
    SECTION("No effect") {
        float effectiveness = getTypeEffectiveness(
            CreatureType::Normal,
            CreatureType::Ghost
        );
        CHECK(effectiveness == 0.0f);
    }
}

TEST_CASE("Creature AI Behavior", "[creature][ai]") {
    CreatureBehaviorSystem behaviorSystem;
    
    auto creature = createTestCreature();
    auto* ai = getComponent<CreatureAIComponent>(creature);
    
    SECTION("Flee from threat") {
        ai->personality.fearfulness = 0.9f;
        ai->personality.aggression = 0.1f;
        
        // Simulate threat detection
        auto threat = createTestPlayer();
        positionEntity(threat, glm::vec3(5, 0, 0));
        
        behaviorSystem.update(0.016f);
        
        CHECK(ai->currentState == CreatureAIComponent::Flee);
    }
    
    SECTION("Wander when idle") {
        ai->currentState = CreatureAIComponent::Idle;
        ai->personality.curiosity = 1.0f;
        
        behaviorSystem.update(1.0f);  // Update for 1 second
        
        CHECK(ai->currentState == CreatureAIComponent::Wander);
        CHECK(ai->targetPosition != ai->homePosition);
    }
}
```

## Performance Benchmarks

```cpp
static void BM_CreatureAI_100_Creatures(benchmark::State& state) {
    CreatureBehaviorSystem system;
    CreatureSpawner spawner;
    
    // Spawn 100 creatures
    for (int i = 0; i < 100; ++i) {
        spawner.spawnCreature(
            1,  // Species ID
            glm::vec3(i * 2, 0, i * 2),
            5   // Level 5
        );
    }
    
    for (auto _ : state) {
        system.update(0.016f);
    }
}
BENCHMARK(BM_CreatureAI_100_Creatures);
```

## Week 4 Daily Milestones

### Day 22-23: Data Structures
- Implement CreatureData structures
- Create species database loader
- Design stat calculation system

### Day 24: Component Architecture  
- Create CreatureComponent
- Implement CreatureAIComponent
- Add battle stats tracking

### Day 25: Basic AI
- Implement state machine
- Create wander behavior
- Add threat detection

### Day 26: Spawning System
- Create spawn zones
- Implement spawn rules
- Add level scaling

### Day 27: Advanced Behaviors
- Implement flee/approach
- Add personality system
- Create social interactions

### Day 28: Testing & Polish
- Complete unit tests
- Run AI benchmarks
- Debug behavior edge cases

## Performance Targets

- **AI Update**: < 5ms for 100 creatures
- **Spawn Check**: < 0.5ms per zone
- **Stat Calculation**: < 0.1ms per creature
- **Memory**: ~2KB per creature
- **Behavior Decisions**: < 0.05ms per creature