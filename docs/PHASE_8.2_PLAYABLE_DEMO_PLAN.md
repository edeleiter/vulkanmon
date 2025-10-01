# Phase 8.2: First Playable Pokemon Demo - Implementation Plan

## 🎯 Mission Statement

**Transform VulkanMon from a feature-complete engine into a playable Pokemon-style game demo.**

Implement the minimal viable gameplay loop: Walk around → See creatures → Throw pokeball → Capture creature → View party.

---

## 🏗️ Architecture Philosophy: Engine vs Game Separation

### **Engine Layer** (`src/` directory)
- Generic, reusable systems
- No Pokemon-specific logic
- Can be used for any game
- Example: `CharacterControllerComponent`, `AIBehaviorComponent`, `CaptureSystem`

### **Game Layer** (`game/` directory)
- Pokemon-specific gameplay logic
- Uses engine components
- Game rules and mechanics
- Example: `PokemonCreatureData`, `PokemonCaptureLogic`, `PokemonPartyManager`

### **Directory Structure**
```
src/
  components/           # Engine components (generic)
    CharacterControllerComponent.h
    AIBehaviorComponent.h
    CreatureStatsComponent.h
  systems/              # Engine systems (generic)
    CharacterControllerSystem.h
    AIBehaviorSystem.h
    CaptureSystem.h

game/                   # Pokemon-specific game logic
  pokemon/
    PokemonSpecies.h    # Pokemon data definitions
    PokemonCreature.h   # Pokemon instance data
    PokemonParty.h      # Party management
  managers/
    PokemonSpawner.h    # Creature spawning logic
    CaptureManager.h    # Pokemon-specific capture rules
  ui/
    PartyUI.h           # Party display UI
```

---

## 📋 Implementation Tasks (Granular Breakdown)

### **Phase 8.2.1: Player Character Controller** (ENGINE)
**Goal**: Generic character controller usable for any third-person game

#### Task 8.2.1.1: CharacterControllerComponent Design
- [ ] Create `src/components/CharacterControllerComponent.h`
  - Movement speed (walk/run speeds)
  - Jump parameters (height, gravity multiplier)
  - Ground detection settings
  - Input response curves
  - Animation state (idle/walk/run/jump) - enum only, no animation system yet
  - **Deliverable**: Component header with well-documented fields

#### Task 8.2.1.2: CharacterControllerSystem Implementation
- [ ] Create `src/systems/CharacterControllerSystem.h/.cpp`
  - `update()` method: Process input and update character transform
  - WASD input → velocity calculation
  - Jump input → vertical impulse via PhysicsSystem
  - Ground detection via physics raycasts
  - Camera-relative movement (forward = camera forward projected to XZ plane)
  - **Deliverable**: Functional character movement system

#### Task 8.2.1.3: Physics Integration
- [ ] Integrate CharacterControllerComponent with PhysicsSystem
  - Create capsule collision shape for player
  - Configure collision layers (Player layer already exists)
  - Apply movement forces via Jolt Physics
  - Handle slopes and stairs (if time permits, otherwise flat terrain only)
  - **Deliverable**: Player entity with physics-based movement

#### Task 8.2.1.4: Camera Follow System
- [ ] Create `src/systems/CameraFollowComponent.h`
  - Target entity to follow
  - Offset from target (distance, height)
  - Smoothing factor (lerp speed)
  - Look-at target vs free look mode
  - **Deliverable**: Component for third-person camera

- [ ] Extend CameraSystem to support follow behavior
  - Update camera position to follow target entity
  - Smooth camera movement (lerp)
  - Optional: Mouse right-click to rotate camera around player
  - **Deliverable**: Working third-person camera

#### Task 8.2.1.5: Player Entity Creation (GAME)
- [ ] Create `game/player/PlayerController.h/.cpp`
  - Factory method: `createPlayer(World&, glm::vec3 spawnPos)`
  - Assembles player entity with all required components:
    - Transform (position, rotation)
    - CharacterControllerComponent
    - RigidBodyComponent (capsule, kinematic)
    - CollisionComponent (Player layer)
    - SpatialComponent (for queries)
    - Renderable (temporary capsule mesh, will be character model later)
  - **Deliverable**: Spawnable player entity

#### Task 8.2.1.6: Input Binding Integration
- [ ] Extend InputHandler to support character movement
  - WASD → CharacterControllerSystem
  - Space → Jump
  - Shift → Run toggle
  - Mouse move → Camera rotation (if implementing free camera)
  - **Deliverable**: Player responds to keyboard/mouse input

#### Task 8.2.1.7: Testing
- [ ] Create `tests_cpp/test_CharacterController.cpp`
  - Test component creation and initialization
  - Test movement velocity calculation
  - Test camera-relative direction conversion
  - Test jump state transitions
  - **Deliverable**: Unit tests for character controller

---

### **Phase 8.2.2: Creature AI Behavior System** (ENGINE)

#### Task 8.2.2.1: AIBehaviorComponent Design
- [ ] Create `src/components/AIBehaviorComponent.h`
  - Behavior state enum (Idle, Wander, Flee, Follow, Aggressive)
  - Movement speed
  - Detection radius (how far can see player/targets)
  - Flee threshold (health % to trigger flee)
  - Wander parameters (radius, wait time, move time)
  - Target entity (what is AI targeting)
  - State machine data (current state, timer, target position)
  - **Deliverable**: Generic AI component for any creature/NPC

#### Task 8.2.2.2: AIBehaviorSystem Implementation
- [ ] Create `src/systems/AIBehaviorSystem.h/.cpp`
  - `update()` method: Update all AI entities
  - State machine implementation:
    - **Idle**: Wait for random time, transition to Wander
    - **Wander**: Pick random point in radius, walk to it, return to Idle
    - **Flee**: Detect player nearby, run away
    - **Follow**: (Optional) Follow target entity
    - **Aggressive**: (Optional) Chase and attack player
  - Use SpatialSystem for player/target detection
  - Apply movement via Transform or PhysicsSystem
  - **Deliverable**: Functional AI behavior system

#### Task 8.2.2.3: Wander Behavior Implementation
- [ ] Implement wander state logic
  - Random point selection in radius around spawn position
  - Pathfinding (simple: straight line to target for MVP)
  - Arrival detection (distance threshold)
  - Wait timer before next wander
  - **Deliverable**: Creatures wander realistically

#### Task 8.2.2.4: Flee Behavior Implementation
- [ ] Implement flee state logic
  - Query SpatialSystem for player within detection radius
  - Calculate flee direction (away from player)
  - Apply flee speed (faster than wander)
  - Return to Idle when player out of range
  - **Deliverable**: Creatures flee from player

#### Task 8.2.2.5: Testing
- [ ] Create `tests_cpp/test_AIBehavior.cpp`
  - Test state transitions (Idle → Wander → Idle)
  - Test wander point selection within radius
  - Test player detection and flee triggering
  - Test movement direction calculations
  - **Deliverable**: Unit tests for AI system

---

### **Phase 8.2.3: Pokemon Creature System** (GAME)

#### Task 8.2.3.1: Pokemon Species Data Definition
- [ ] Create `game/pokemon/PokemonSpecies.h`
  - Species ID enum (BULBASAUR = 1, CHARMANDER = 4, SQUIRTLE = 7, PIKACHU = 25, etc.)
  - Species data struct:
    - Name
    - Base stats (HP, Attack, Defense - simplified)
    - Model path (mesh file)
    - Capture rate (0.0-1.0, chance to catch)
    - Size/scale
    - AI behavior preset (some flee, some aggressive, some ignore player)
  - **Deliverable**: Data-driven Pokemon species definitions

#### Task 8.2.3.2: Pokemon Creature Component
- [ ] Create `game/pokemon/PokemonCreature.h` (component)
  - Species ID reference
  - Individual stats (current HP, level - start at level 5)
  - Nickname (optional, defaults to species name)
  - Original Trainer (who caught it)
  - Captured flag (wild vs caught)
  - **Deliverable**: Component for Pokemon instances

#### Task 8.2.3.3: Pokemon Spawner System
- [ ] Create `game/managers/PokemonSpawner.h/.cpp`
  - Factory method: `spawnWildPokemon(World&, SpeciesID, glm::vec3 position)`
  - Assembles Pokemon entity with:
    - Transform
    - PokemonCreature component
    - AIBehaviorComponent (species-appropriate behavior)
    - RigidBodyComponent (sphere or capsule)
    - CollisionComponent (Creatures layer)
    - SpatialComponent (for detection)
    - Renderable (species model)
  - **Deliverable**: Spawnable wild Pokemon

#### Task 8.2.3.4: Demo Scene Population
- [ ] Modify `ApplicationSetup.cpp` to spawn Pokemon
  - Replace physics demo objects with Pokemon
  - Spawn 5-10 wild Pokemon in scene
  - Mix of species (Pikachu, Bulbasaur, Charmander, Squirtle, etc.)
  - Spread around player spawn position (5-20 units away)
  - **Deliverable**: Scene with wild Pokemon wandering

#### Task 8.2.3.5: Species Data Implementation
- [ ] Implement 5 starter Pokemon species
  - Pikachu (flees from player, fast)
  - Bulbasaur (wanders slowly, easy to catch)
  - Charmander (wanders, medium difficulty)
  - Squirtle (wanders, medium difficulty)
  - Eevee (wanders randomly, easy to catch)
  - Each with appropriate stats and behavior
  - **Deliverable**: 5 functional Pokemon species

---

### **Phase 8.2.4: Pokeball Capture Mechanic** (GAME + ENGINE)

#### Task 8.2.4.1: CaptureComponent Design (ENGINE)
- [ ] Create `src/components/CaptureComponent.h`
  - Capturable flag (is this entity capturable?)
  - Capture rate (base chance 0.0-1.0)
  - Capture state enum (Wild, BeingCaptured, Captured, Failed)
  - Capture timer (for animation/shake effect)
  - Capture device entity (which pokeball hit me)
  - **Deliverable**: Generic capture component for any game

#### Task 8.2.4.2: CaptureSystem Implementation (ENGINE)
- [ ] Create `src/systems/CaptureSystem.h/.cpp`
  - Detect projectile collisions with capturable entities
  - Trigger capture attempt on collision
  - Calculate capture success (random roll vs capture rate)
  - Trigger capture animation/effect (stub for MVP)
  - Notify game layer of capture result via callback
  - **Deliverable**: Generic capture system

#### Task 8.2.4.3: Pokeball Projectile Configuration (GAME)
- [ ] Extend ProjectileComponent for Pokeballs
  - Already have `ProjectileComponent::createPokeball()` factory
  - Add collision callback to detect creature hits
  - Configure to use CaptureDevices layer
  - Set collision mask to hit Creatures layer
  - **Deliverable**: Pokeball projectiles trigger captures

#### Task 8.2.4.4: Pokemon Capture Manager (GAME)
- [ ] Create `game/managers/PokemonCaptureManager.h/.cpp`
  - Handle capture success callback from CaptureSystem
  - Calculate Pokemon-specific capture rate:
    - Base capture rate from species
    - Modified by Pokemon HP% (lower HP = easier)
    - Modified by Pokemon level (higher level = harder)
    - Apply pokeball multiplier (regular pokeball = 1.0x)
  - On success: Add Pokemon to party, remove from world
  - On failure: Pokemon breaks free, continue as wild
  - **Deliverable**: Pokemon-specific capture logic

#### Task 8.2.4.5: Capture Feedback
- [ ] Add visual/audio feedback for capture attempts
  - Console log: "Gotcha! Pikachu was caught!"
  - Console log: "Oh no! The Pokemon broke free!"
  - (Optional) Simple particle effect on successful capture
  - **Deliverable**: Player knows if capture succeeded

#### Task 8.2.4.6: Testing
- [ ] Create `tests_cpp/test_CaptureSystem.cpp`
  - Test capture rate calculation
  - Test collision detection with capturables
  - Test capture state transitions
  - Test successful capture removes entity
  - **Deliverable**: Unit tests for capture system

---

### **Phase 8.2.5: Party Management System** (GAME)

#### Task 8.2.5.1: Pokemon Party Manager
- [ ] Create `game/pokemon/PokemonParty.h/.cpp`
  - Party storage (std::array<PokemonData, 6>)
  - Add Pokemon to party (fail if full)
  - Remove Pokemon from party
  - Get party count
  - Get Pokemon by index
  - Serialize/deserialize party (for save system later)
  - **Deliverable**: Party management logic

#### Task 8.2.5.2: Pokemon Data Persistence
- [ ] Create `game/pokemon/PokemonData.h`
  - Struct to store caught Pokemon data:
    - Species ID
    - Nickname
    - Level
    - Current HP / Max HP
    - Stats
    - Moves (stub for now, just names)
    - Original Trainer name
  - **Deliverable**: Data structure for caught Pokemon

#### Task 8.2.5.3: Party Integration with Capture
- [ ] Connect PokemonCaptureManager to PokemonParty
  - On successful capture, create PokemonData from wild Pokemon
  - Add to party via PokemonParty::addPokemon()
  - Handle party full case (for MVP: just fail capture)
  - **Deliverable**: Captured Pokemon go into party

---

### **Phase 8.2.6: Party UI Display** (GAME)

#### Task 8.2.6.1: Party UI Component Design
- [ ] Create `game/ui/PartyUI.h/.cpp`
  - ImGui window showing party members
  - Display for each Pokemon:
    - Species name / Nickname
    - Level
    - HP bar (current/max)
    - Sprite/icon (optional, text is fine for MVP)
  - Toggle key (P key to show/hide)
  - **Deliverable**: Party UI window

#### Task 8.2.6.2: ImGui Integration
- [ ] Integrate PartyUI with existing ECSInspector
  - Add PartyUI rendering to Application::renderUI()
  - Access PokemonParty from Application
  - Update UI every frame
  - **Deliverable**: Functional party display

#### Task 8.2.6.3: Capture Notification UI
- [ ] Add temporary notification system
  - Display "Gotcha! Pikachu was caught!" for 3 seconds
  - Display "Pokemon broke free!" for 2 seconds
  - Simple ImGui notification overlay
  - **Deliverable**: Visual capture feedback

---

### **Phase 8.2.7: Environment/Terrain Setup** (GAME)

#### Task 8.2.7.1: Ground Plane Creation
- [ ] Create larger ground plane model
  - 50x50 unit plane (vs current small test plane)
  - Simple grass texture (use existing or solid green)
  - Static physics collision
  - **Deliverable**: Playable terrain

#### Task 8.2.7.2: Environment Bounds
- [ ] Configure world bounds for gameplay
  - Set SpatialManager bounds to (-50, -5, -50) to (50, 20, 50)
  - Add invisible collision walls at boundaries (optional)
  - **Deliverable**: Defined play area

#### Task 8.2.7.3: Scene Setup
- [ ] Create demo scene layout in ApplicationSetup
  - Spawn player at (0, 2, 0)
  - Spawn 8-10 Pokemon scattered around (5-30 unit radius)
  - Camera positioned behind player (third-person view)
  - Remove all physics demo objects
  - **Deliverable**: Clean Pokemon demo scene

---

### **Phase 8.2.8: Input & Controls Polish**

#### Task 8.2.8.1: Control Documentation
- [ ] Document all controls in console at startup
  - WASD: Move player
  - Space: Jump
  - Shift: Run (if implemented)
  - Mouse: Rotate camera (if implemented)
  - Left Click: Throw pokeball
  - P: Toggle party UI
  - I: Toggle ECS inspector
  - **Deliverable**: Player knows how to play

#### Task 8.2.8.2: Control Scheme Refinement
- [ ] Test and refine control responsiveness
  - Adjust character movement speed for good feel
  - Adjust pokeball throw speed/arc
  - Adjust camera follow smoothness
  - **Deliverable**: Controls feel good

---

### **Phase 8.2.9: Testing & Validation**

#### Task 8.2.9.1: Full Gameplay Loop Test
- [ ] Manual testing: Complete gameplay loop
  - Start application
  - See player and wild Pokemon
  - Walk around using WASD
  - Approach Pokemon (some should flee)
  - Throw pokeball at Pokemon
  - Successfully capture Pokemon
  - Press P, see Pokemon in party
  - Repeat until party has 3-4 Pokemon
  - **Deliverable**: Validated gameplay loop

#### Task 8.2.9.2: Performance Testing
- [ ] Test with 10-20 wild Pokemon in scene
  - Measure FPS (should maintain 60+)
  - Check AI update time (should be <2ms)
  - Check capture system overhead
  - **Deliverable**: Performance validated

#### Task 8.2.9.3: Edge Case Testing
- [ ] Test edge cases:
  - Party full (6 Pokemon) - what happens on capture?
  - No Pokemon nearby - pokeball just flies away
  - Multiple Pokemon in one area
  - Capturing fleeing Pokemon
  - **Deliverable**: Bugs identified and fixed

#### Task 8.2.9.4: Unit Test Coverage
- [ ] Ensure all new systems have tests:
  - CharacterControllerSystem
  - AIBehaviorSystem
  - CaptureSystem
  - PokemonParty
  - **Deliverable**: Test suite passes (2000+ assertions)

---

### **Phase 8.2.10: Documentation**

#### Task 8.2.10.1: Update CLAUDE.md
- [ ] Add Phase 8.2 completion status
  - Document new systems
  - Update current status section
  - Add gameplay controls
  - **Deliverable**: Updated project documentation

#### Task 8.2.10.2: Create Gameplay Demo Video Plan
- [ ] Document demo video sequence
  - 0:00-0:10 - Show player and wild Pokemon
  - 0:10-0:30 - Walk around, show camera
  - 0:30-0:50 - Throw pokeballs, show captures
  - 0:50-1:00 - Show party UI with caught Pokemon
  - **Deliverable**: Video shooting script

#### Task 8.2.10.3: API Documentation
- [ ] Document new engine APIs
  - CharacterControllerSystem usage
  - AIBehaviorSystem configuration
  - CaptureSystem integration
  - **Deliverable**: API docs for engine users

---

## 📊 Success Metrics

### **Functional Requirements**
- [ ] Player can walk around with WASD
- [ ] Camera follows player in third-person view
- [ ] 5-10 wild Pokemon wander in scene
- [ ] Some Pokemon flee when player approaches
- [ ] Player can throw pokeballs with mouse click
- [ ] Pokeball collision triggers capture attempt
- [ ] Successful capture adds Pokemon to party
- [ ] Press P to see party with caught Pokemon
- [ ] All 1993+ existing tests still pass

### **Performance Requirements**
- [ ] 60+ FPS with 10 wild Pokemon
- [ ] <2ms AI system update time
- [ ] <1ms capture system update time
- [ ] No memory leaks after 100+ captures

### **Quality Requirements**
- [ ] Controls feel responsive
- [ ] Pokemon behavior looks natural
- [ ] Capture feedback is clear
- [ ] Party UI is readable
- [ ] Code follows existing architecture patterns

---

## 🎯 Estimated Timeline

### **Session 1: Player & Camera (4-6 hours)**
- Tasks 8.2.1.1 - 8.2.1.7: Player controller
- Deliverable: Playable character walking around

### **Session 2: Creatures & AI (4-5 hours)**
- Tasks 8.2.2.1 - 8.2.2.5: AI behavior system
- Tasks 8.2.3.1 - 8.2.3.5: Pokemon creatures
- Deliverable: Wild Pokemon wandering in scene

### **Session 3: Capture & Party (3-4 hours)**
- Tasks 8.2.4.1 - 8.2.4.6: Capture system
- Tasks 8.2.5.1 - 8.2.5.3: Party management
- Tasks 8.2.6.1 - 8.2.6.3: Party UI
- Deliverable: Working capture mechanic

### **Session 4: Polish & Testing (2-3 hours)**
- Tasks 8.2.7.1 - 8.2.7.3: Environment
- Tasks 8.2.8.1 - 8.2.8.2: Controls polish
- Tasks 8.2.9.1 - 8.2.9.4: Testing
- Tasks 8.2.10.1 - 8.2.10.3: Documentation
- Deliverable: Polished demo ready to show

**Total Estimated Time**: 13-18 hours (3-4 development sessions)

---

## 🏗️ Architecture Validation

### **Engine/Game Separation Checklist**
- [ ] All `src/components/` are game-agnostic
- [ ] All `src/systems/` are game-agnostic
- [ ] All Pokemon-specific logic in `game/` directory
- [ ] Game layer can be swapped for different game without touching `src/`
- [ ] Engine systems have no knowledge of Pokemon

### **Integration Points**
- [ ] CharacterControllerSystem ← InputHandler (existing)
- [ ] AIBehaviorSystem ← SpatialSystem (existing)
- [ ] CaptureSystem ← ProjectileSystem (existing)
- [ ] PokemonSpawner ← World/EntityManager (existing)
- [ ] PartyUI ← ImGui (existing)

---

## 📝 Notes

### **Scope Decisions (MVP)**
**IN SCOPE** (must have):
- Player movement (walk only)
- Basic AI (wander + flee)
- Pokeball throwing
- Simple capture mechanic
- Party storage & display (up to 6)
- 5 Pokemon species

**OUT OF SCOPE** (nice to have, defer):
- Running/sprinting
- Jumping
- Advanced AI (follow, aggressive, patrol routes)
- Battle system
- Pokemon stats/moves/abilities
- Multiple pokeball types
- Capture animations
- Pokemon cries/sounds
- Save/load system
- Multiple areas/maps
- NPCs/dialogue
- Items/inventory

### **Asset Requirements**
- [ ] Player capsule mesh (temporary, use built-in)
- [ ] 5 Pokemon models (simple shapes for MVP, can be spheres/cubes with different colors)
- [ ] Ground plane texture (solid green is fine)
- [ ] Pokeball model (already have projectile_sphere.obj)

### **Technical Notes**
- Use existing PhysicsSystem for all movement (player + Pokemon)
- Use existing SpatialSystem for detection/queries
- Use existing ProjectileSystem for pokeballs
- Use existing ImGui integration for UI
- No new external dependencies needed

---

## 🎮 Demo Script (2-Minute Gameplay Loop)

**Minute 0:00-0:30**: Walk around field
- Start game, see player character
- Press WASD, player walks around
- Camera follows from behind
- See 5+ Pokemon wandering nearby

**Minute 0:30-1:00**: Pokemon interaction
- Walk toward Pikachu
- Pikachu flees when player gets close
- Walk toward Bulbasaur (doesn't flee)
- Bulbasaur wanders randomly

**Minute 1:00-1:30**: Capture Pokemon
- Left click to throw pokeball at Bulbasaur
- "Gotcha! Bulbasaur was caught!" appears
- Bulbasaur disappears from world

**Minute 1:30-2:00**: View party
- Press P to open party UI
- See Bulbasaur in party slot 1
- Shows level, HP, species name
- Capture 1-2 more Pokemon
- Party now shows 2-3 Pokemon

**Demo Complete**: Player has experienced full gameplay loop!

---

*This plan transforms VulkanMon from a professional engine into a playable Pokemon-style game, validating 7+ phases of engine development with actual gameplay.*
