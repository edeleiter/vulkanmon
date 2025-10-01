# VulkanMon Projectile System Implementation Plan

## 🎯 **Strategic Analysis: What Are We Actually Trying to Accomplish?**

### **Core Goal**
Implement a mouse-click projectile spawning system for VulkanMon that enables Pokemon-style gameplay mechanics (Pokeball throwing, battle projectiles, etc.).

### **Key Success Criteria**
1. ✅ **Mouse Click** → Projectile spawns from camera position in mouse direction
2. ✅ **Physics Simulation** → Projectile follows realistic trajectory with gravity
3. ✅ **Visual Representation** → Projectile appears as 3D sphere with material
4. ✅ **Collision Response** → Projectile bounces/sticks/destroys on impact
5. ✅ **ECS Integration** → Works seamlessly with existing World/EntityManager
6. ✅ **Performance** → No regressions, maintains 60+ FPS

### **Critical Problems with Previous Approach**
- ❌ **Assumed APIs**: Built complex system without understanding actual ECS interfaces
- ❌ **Too Ambitious**: Tried to implement everything at once vs incremental approach
- ❌ **No Validation**: Didn't test basic compilation before adding complexity
- ❌ **API Mismatches**: EntityManager, CameraSystem, MaterialSystem APIs different than expected

---

## 📋 **Phase-by-Phase Implementation Strategy**

### **Phase 1: Architecture Analysis** ⚡ *CRITICAL FOUNDATION*
**Goal**: Understand existing ECS architecture and APIs before writing any code

#### **Phase 1.1: ECS API Discovery**
- [ ] **Examine EntityManager.h** - Document actual API methods for entity/component management
- [ ] **Examine CameraSystem.h** - Document screenToWorldRay() signature and Ray structure
- [ ] **Examine existing Systems** - See how RenderSystem, PhysicsSystem integrate with ECS
- [ ] **Document System Base Classes** - Understand SystemImpl.h and System<> template

#### **Phase 1.2: Component Architecture Review**
- [ ] **Review Transform component** - Understand position/rotation/scale structure
- [ ] **Review Renderable component** - Understand meshPath/materialId structure
- [ ] **Review Physics components** - RigidBodyComponent and CollisionComponent actual structure
- [ ] **Verify ProjectileComponent** - Ensure it matches ECS component conventions

#### **Phase 1.3: Input System Integration**
- [ ] **Examine InputHandler** - Understand mouse button callback system
- [ ] **Examine Window class** - Understand mouse event flow
- [ ] **Find working example** - See how existing input callbacks work

**Success Criteria**: Clear documentation of ALL APIs needed for projectile system

---

### **Phase 2: Minimal Working Implementation** 🔧 *BUILD FOUNDATION*
**Goal**: Create simplest possible ProjectileSystem that compiles and runs

#### **Phase 2.1: Minimal ProjectileSystem**
- [ ] **Simplify ProjectileSystem.h** - Remove complex features, focus on basic spawning
- [ ] **Implement basic constructor** - Just store CameraSystem reference
- [ ] **Implement stub update()** - Empty method that compiles
- [ ] **Implement basic spawnProjectile()** - Create entity with Transform + Renderable only

#### **Phase 2.2: Integration Testing**
- [ ] **Add to CMakeLists.txt** - Ensure compilation
- [ ] **Connect to Application** - Add system to ECS World
- [ ] **Verify compilation** - Fix any remaining API mismatches
- [ ] **Test empty run** - Ensure no crashes when system is present

**Success Criteria**: Application compiles and runs with ProjectileSystem present (but non-functional)

---

### **Phase 3: Basic Projectile Spawning** 🚀 *CORE FUNCTIONALITY*
**Goal**: Get mouse-click projectile spawning working with basic visuals

#### **Phase 3.1: Entity Creation**
- [ ] **Implement createProjectileEntity()** - Basic Transform + Renderable entity creation
- [ ] **Use simple sphere model** - assets/models/projectile_sphere.obj
- [ ] **Use default material** - materialId = 0
- [ ] **Test entity creation** - Verify projectiles appear in scene

#### **Phase 3.2: Mouse Input Integration**
- [ ] **Add mouse button callback** - Connect Window → InputHandler → ProjectileSystem
- [ ] **Implement basic camera ray** - Use CameraSystem to convert mouse to world direction
- [ ] **Spawn at camera position** - Simple projectile creation on mouse click
- [ ] **Test mouse spawning** - Verify mouse clicks create visible projectiles

**Success Criteria**: Mouse clicks spawn visible spheres in 3D space

---

### **Phase 4: Physics Integration** ⚖️ *REALISTIC SIMULATION*
**Goal**: Add physics simulation for realistic projectile trajectory

#### **Phase 4.1: Simple Physics**
- [ ] **Add velocity to ProjectileComponent** - Simple velocity vector
- [ ] **Implement basic movement** - Update position based on velocity in update()
- [ ] **Add gravity** - Simple downward acceleration
- [ ] **Test basic trajectory** - Verify projectiles follow parabolic path

#### **Phase 4.2: Jolt Physics Integration**
- [ ] **Add RigidBodyComponent** - Connect to existing physics system
- [ ] **Add CollisionComponent** - Simple sphere collision
- [ ] **Test physics simulation** - Verify Jolt Physics handles projectile movement
- [ ] **Implement collision response** - Basic bounce/destroy behavior

**Success Criteria**: Projectiles follow realistic physics trajectory and collision

---

### **Phase 5: Polish & Enhancement** ✨ *FULL FEATURES*
**Goal**: Add remaining features and polish the system

#### **Phase 5.1: Projectile Types**
- [ ] **Add ProjectileComponent types** - Bullet, Grenade, Pokeball, etc.
- [ ] **Implement material switching** - Different visuals per type
- [ ] **Add type-specific behavior** - Different physics properties per type

#### **Phase 5.2: Advanced Features**
- [ ] **Add lifetime management** - Auto-cleanup expired projectiles
- [ ] **Add collision callbacks** - Events for impact detection
- [ ] **Add trajectory prediction** - Visual trajectory lines
- [ ] **Performance optimization** - Efficient cleanup and collision

**Success Criteria**: Complete projectile system matching original design goals

---

## 🎮 **Projectile Test Scene Implementation**

### **Phase 6: Comprehensive Test Scene** 🧪 *VALIDATION ENVIRONMENT*
**Goal**: Create dedicated test scene for validating all projectile system functionality

#### **Phase 6.1: Test Scene Architecture**
- [ ] **Create ProjectileTestScene class** - Dedicated scene manager for projectile testing
- [ ] **Add scene switching mechanism** - Toggle between physics demo and projectile test
- [ ] **Implement scene cleanup** - Proper entity cleanup when switching scenes
- [ ] **Add scene state management** - Track active scene type in Application

#### **Phase 6.2: Target Objects Setup**
- [ ] **Create static target walls** - Large vertical planes at varying distances (10m, 20m, 30m)
  - Position: (0,5,10), (0,5,20), (0,5,30)
  - Materials: Different colors (Red, Green, Blue) for distance identification
  - Collision: Static collision boxes for projectile impacts
- [ ] **Add moving targets** - Oscillating entities for accuracy testing
  - Horizontal movers: Side-to-side motion across different speeds
  - Vertical movers: Up-down motion for elevation testing
  - Circular movers: Orbital patterns for complex tracking
- [ ] **Implement target health system** - Visual feedback for projectile hits
  - Hit counter display per target
  - Color changes on impact (flash white, return to base color)
  - Destruction after N hits for respawn testing

#### **Phase 6.3: Environmental Test Elements**
- [ ] **Add bounce surfaces** - High-restitution platforms for bounce testing
  - Angled platforms: 30°, 45°, 60° slopes for trajectory reflection
  - Material properties: Very high bounce coefficient (0.9+)
  - Visual indicators: Metallic/mirror materials
- [ ] **Create absorption zones** - Areas that stop projectiles completely
  - Soft materials: Low restitution (0.1) for impact absorption
  - Trigger zones: Destroy projectiles on entry for cleanup testing
  - Visual cues: Foam-like or fabric textures
- [ ] **Implement wind zones** - Areas with constant force application
  - Directional forces: Left/right/up/down affecting projectile trajectory
  - Force visualization: Particle effects or arrow indicators
  - Variable intensity: Different zones with different force magnitudes

#### **Phase 6.4: Projectile Testing Infrastructure**
- [ ] **Add projectile type cycling** - Keyboard shortcuts for switching projectile types
  - Number keys 1-6: Bullet, Grenade, Arrow, Magic, Pokeball, Physics Object
  - Visual UI indicator: Current projectile type display
  - Parameter display: Show current type's physics properties
- [ ] **Implement rapid-fire mode** - Hold mouse button for continuous spawning
  - Configurable fire rate: 1-10 projectiles per second
  - Automatic spacing: Prevent overlapping spawns
  - Ammo limiting: Max projectiles per burst to prevent performance issues
- [ ] **Add trajectory prediction** - Visual line showing predicted path
  - Dotted line rendering: Show parabolic arc for gravity-affected projectiles
  - Impact point marking: Red sphere at predicted collision point
  - Real-time updates: Line follows mouse cursor movement

#### **Phase 6.5: Performance Testing Setup**
- [ ] **Implement stress testing controls** - Spawn large numbers of projectiles
  - Batch spawn mode: Create 10/50/100 projectiles simultaneously
  - Performance monitoring: FPS counter during stress tests
  - Memory tracking: Monitor entity count and cleanup efficiency
- [ ] **Add collision stress testing** - Dense target arrangements
  - Collision walls: Closely packed target objects
  - Overlap testing: Projectiles hitting multiple targets simultaneously
  - Chain reactions: Targets that spawn more projectiles on hit
- [ ] **Create cleanup validation** - Verify proper entity destruction
  - Manual cleanup button: Force destroy all projectiles
  - Automatic cleanup timer: Remove old projectiles after time limit
  - Memory leak detection: Monitor entity count trends

#### **Phase 6.6: Debug Visualization Tools**
- [ ] **Add collision debug rendering** - Show collision bounds visually
  - Wireframe spheres: Show projectile collision radius
  - Collision event markers: Visual indicators at impact points
  - Velocity vectors: Arrows showing projectile velocity direction/magnitude
- [ ] **Implement physics debug overlay** - Real-time physics data display
  - Projectile count: Total active projectiles
  - Physics stats: Average velocity, total kinetic energy
  - Collision stats: Hits per second, bounce count
- [ ] **Add performance metrics display** - Real-time performance data
  - Frame time graph: Rolling average of frame times
  - Entity count graph: Track entity creation/destruction
  - Physics step timing: Time spent in physics simulation

#### **Phase 6.7: User Interface & Controls**
- [ ] **Create test scene UI panel** - ImGui interface for test controls
  - Projectile type selector: Dropdown or button grid
  - Physics parameter sliders: Mass, gravity, drag, bounce
  - Spawn controls: Single shot, rapid fire, batch spawn
  - Scene controls: Reset, cleanup, scene switching
- [ ] **Add input help overlay** - Show available keyboard shortcuts
  - Key mapping display: What each key/button does
  - Context-sensitive help: Different help for different modes
  - Toggle visibility: F1 key to show/hide help
- [ ] **Implement camera controls for testing** - Optimal viewing angles
  - Preset camera positions: Overview, side view, target view
  - Follow mode: Camera follows active projectiles
  - Free look mode: Standard WASD camera for custom positioning

#### **Phase 6.8: Scene Data & Configuration**
- [ ] **Create scene configuration file** - JSON/data file for test scene setup
  - Target positions and properties
  - Environment element configurations
  - Default projectile settings
  - Camera preset positions
- [ ] **Implement scene presets** - Multiple test scenarios
  - Accuracy test: Static targets at various distances
  - Physics test: Bounce surfaces and gravity wells
  - Stress test: Dense collision environments
  - Gameplay test: Pokemon-style capture scenarios
- [ ] **Add scene persistence** - Save/load test configurations
  - Export test results: Performance data, hit statistics
  - Import custom scenes: Load community-created test environments
  - Session state: Remember last used settings

### **Test Scene Integration with Main Application**

#### **Scene Management Integration**
- [ ] **Modify Application.cpp** - Add scene switching logic
- [ ] **Update createTestScene()** - Choose between physics demo and projectile test
- [ ] **Add scene transition** - Smooth switching between test environments
- [ ] **Preserve existing scenes** - Keep physics demo available via key toggle

#### **Input System Integration**
- [ ] **Add scene-specific input handling** - Different controls per scene
- [ ] **Implement input mode switching** - Disable conflicting controls
- [ ] **Add test scene shortcuts** - Quick access to common test functions

#### **Performance Integration**
- [ ] **Ensure no regressions** - Test scene doesn't impact existing performance
- [ ] **Optimize for testing** - Efficient entity management for high projectile counts
- [ ] **Monitor resource usage** - Validate memory and CPU usage during tests

---

## 🔧 **Critical Implementation Guidelines**

### **Follow VulkanMon Design Philosophy**
1. **Simple is Powerful** - Start minimal, add complexity incrementally
2. **Test as we go** - Validate each phase before proceeding
3. **Document often** - Update progress and findings

### **API-First Development**
- **NEVER assume APIs** - Always check actual method signatures
- **Copy existing patterns** - Follow how other systems integrate with ECS
- **Validate early** - Test compilation after each small change

### **Risk Mitigation**
- **Incremental commits** - Commit working state after each phase
- **Fallback plan** - If phase fails, revert and try simpler approach
- **API documentation** - Keep running notes on actual API discoveries

---

## 📊 **Progress Tracking**

### **Current Status**: Phase 1 (Architecture Analysis)
- **Completion**: 0% of Phase 1
- **Blockers**: Need to understand actual ECS APIs
- **Next Action**: Examine EntityManager.h API

### **Timeline Estimate**
- **Phase 1**: 1 session (architecture analysis)
- **Phase 2**: 1 session (minimal implementation)
- **Phase 3**: 1-2 sessions (basic spawning)
- **Phase 4**: 1-2 sessions (physics integration)
- **Phase 5**: 1-2 sessions (polish & features)
- **Total**: 5-8 development sessions

---

## 🎯 **Success Metrics**

### **Technical Metrics**
- ✅ **Compilation**: No build errors
- ✅ **Performance**: Maintains 60+ FPS with 20+ projectiles
- ✅ **Integration**: No regressions in existing systems
- ✅ **Test Coverage**: All 102 existing tests still pass

### **Gameplay Metrics**
- ✅ **Responsiveness**: Mouse click spawns projectile within 16ms
- ✅ **Visual Quality**: Projectile follows realistic trajectory
- ✅ **Interaction**: Collision detection works reliably
- ✅ **Types**: Multiple projectile types work as expected

---

*This document serves as the strategic roadmap for VulkanMon projectile system implementation. Each phase builds incrementally toward the final goal while maintaining system stability and following established architecture patterns.*