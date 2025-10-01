# Phase 8.1: Generic Projectile System Implementation Todo

## Overview
Implementation plan for VulkanMon's generic projectile system - a configurable, performance-optimized engine subsystem for projectile-based gameplay mechanics.

## Design Goals
- **Engine-Generic**: Reusable for bullets, grenades, arrows, magic spells, physics objects
- **Configurable Physics**: Velocity, gravity, drag, bounce, collision response parameters
- **Performance Optimized**: Efficient lifecycle management and collision detection
- **Inspector Integration**: Real-time parameter tuning and debugging capabilities
- **Layer-Based Collision**: Flexible interaction rules via existing layer system

---

## Implementation Tasks

### Phase 8.1.1: Research & Design (4 tasks)
- [ ] **Research existing input handling for mouse clicks and camera ray projection**
  - Examine current InputHandler implementation
  - Review camera system for ray casting capabilities
  - Document existing input event flow

- [ ] **Design ProjectileComponent with velocity, lifetime, and trajectory data**
  - Define component data structure
  - Plan physics integration points
  - Design lifetime management approach

- [ ] **Create ProjectileComponent header with configurable physics properties**
  - Implement component class with proper ECS integration
  - Add physics parameters (velocity, gravity, drag, bounce)
  - Include collision response configuration

- [ ] **Design configurable projectile material system with parameter control**
  - Plan material parameter exposure
  - Design runtime material switching
  - Integration with existing MaterialSystem

### Phase 8.1.2: Input & Camera Integration (2 tasks)
- [ ] **Implement mouse click detection in InputHandler for projectile spawning**
  - Add mouse button event handling
  - Integrate with existing input system
  - Support both left/right click options

- [ ] **Add camera ray projection method to convert screen coordinates to world space**
  - Implement screen-to-world ray casting
  - Support multiple camera entities
  - Handle viewport transformations

### Phase 8.1.3: Assets & Visuals (2 tasks)
- [ ] **Create generic sphere projectile model for engine testing**
  - Generate or source basic sphere model
  - Ensure proper UV mapping for materials
  - Add to assets directory with documentation

- [ ] **Design configurable projectile material system with parameter control**
  - Create material presets for different projectile types
  - Enable runtime material parameter adjustment
  - Integration with ECS Inspector

### Phase 8.1.4: Core System Implementation (6 tasks)
- [ ] **Implement ProjectileSystem for managing projectile entity lifecycle**
  - Create ECS system class
  - Handle entity creation/destruction
  - Integrate with World and EntityManager

- [ ] **Add projectile spawning logic with configurable initial velocity calculation**
  - Implement spawn API methods
  - Calculate initial velocity from input/parameters
  - Support multiple spawn modes (mouse, API, scripted)

- [ ] **Integrate projectile physics with Jolt Physics using configurable collision shapes**
  - Add RigidBodyComponent integration
  - Support sphere, box, capsule collision shapes
  - Configure physics material properties

- [ ] **Implement collision layer filtering system for projectile interactions**
  - Extend existing layer system for projectiles
  - Configure collision rules (what hits what)
  - Support pass-through and solid collision modes

- [ ] **Add projectile lifetime management with configurable TTL and cleanup**
  - Implement time-to-live system
  - Automatic entity cleanup on expiration
  - Distance-based cleanup option

- [ ] **Implement collision event system for projectile impact detection**
  - Create collision callback system
  - Support multiple response types (bounce/stick/destroy)
  - Event notification for game logic integration

### Phase 8.1.5: Physics & Visual Polish (2 tasks)
- [ ] **Add configurable physics response for projectile impacts (bounce/stick/destroy)**
  - Implement bounce coefficient system
  - Add stick-to-surface mechanics
  - Support projectile destruction on impact

- [ ] **Validate projectile physics realism with configurable gravity and drag**
  - Implement air resistance/drag simulation
  - Configurable gravity scaling
  - Trajectory prediction capabilities

### Phase 8.1.6: Testing & Validation (4 tasks)
- [ ] **Create unit tests for ProjectileComponent and physics behavior**
  - Test component data integrity
  - Validate physics parameter ranges
  - Test collision shape creation

- [ ] **Write integration tests for input-to-projectile workflow**
  - Test mouse click to projectile spawn
  - Validate camera ray projection accuracy
  - Test multiple projectile scenarios

- [ ] **Test projectile system from different camera angles and spawn positions**
  - Validate ray projection accuracy
  - Test edge cases (extreme angles, distances)
  - Performance testing with multiple cameras

- [ ] **Performance test with multiple projectiles and collision detection**
  - Stress test with 100+ simultaneous projectiles
  - Measure collision detection overhead
  - Validate cleanup performance

### Phase 8.1.7: Debug Tools & Documentation (3 tasks)
- [ ] **Add engine logging for projectile system events and performance metrics**
  - Log projectile creation/destruction
  - Track collision events and performance
  - Integration with existing Logger system

- [ ] **Update ECS Inspector to display and edit ProjectileComponent properties**
  - Add ProjectileComponent to inspector UI
  - Real-time parameter editing
  - Visual feedback for projectile entities

- [ ] **Create projectile system API documentation for engine users**
  - Document public API methods
  - Provide usage examples
  - Performance guidelines and best practices

### Phase 8.1.8: Final Integration (1 task)
- [ ] **Run full test suite to ensure no regressions in existing engine systems**
  - Execute all 102 test cases
  - Verify 1724 assertions pass
  - Performance regression testing

---

## Success Criteria
- [ ] **Functional**: Mouse click spawns projectile with realistic physics
- [ ] **Configurable**: All physics parameters adjustable via inspector
- [ ] **Performance**: 60+ FPS with 50+ simultaneous projectiles
- [ ] **Generic**: System works for multiple projectile types
- [ ] **Tested**: 100% unit test coverage for new components
- [ ] **Documented**: Complete API documentation for engine users

## Technical Integration Points
- **ECS Architecture**: ProjectileComponent, ProjectileSystem
- **Physics Engine**: Jolt Physics integration with collision shapes
- **Input System**: Mouse click detection and camera ray projection
- **Rendering**: Integration with existing VulkanRenderer and MaterialSystem
- **Debug Tools**: ECS Inspector integration for real-time editing
- **Performance**: Spatial system integration for efficient collision queries

## Estimated Completion
**Total Tasks**: 23 implementation tasks
**Estimated Timeline**: 3-4 development sessions
**Testing Phase**: 1 dedicated testing session
**Documentation**: 1 session for API docs and examples

---

*This todo file tracks the complete implementation of VulkanMon's generic projectile system, designed to be a reusable engine subsystem for any projectile-based gameplay mechanics.*