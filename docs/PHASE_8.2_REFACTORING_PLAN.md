# Phase 8.2 Pre-Implementation Refactoring Plan

## 🎯 Goal: Clean Engine/Game Separation Before Building Gameplay

**Purpose**: Reorganize existing files to establish clear boundaries between generic engine code and Pokemon-specific game code BEFORE implementing Phase 8.2 gameplay features.

**Philosophy**: Following "Simple is Powerful" - clean architecture now prevents confusion later.

---

## 📊 Current State Analysis

### **Existing Files in Wrong Location**

#### **src/game/** (Currently in engine `src/`)
- ❌ `CreatureDetectionSystem.h` - Pokemon-specific creature AI
- ❌ `GrassSystem.h` - Pokemon-specific grass encounter mechanics
- ❌ `CreatureComponent` (in CreatureDetectionSystem.h) - Pokemon creature data

**Problem**: These are **Pokemon game logic**, not generic engine features, but living in `src/` suggests they're engine code.

#### **src/examples/** (Demo code in engine)
- ❌ `ECS_Example.cpp` - Example code that shouldn't ship with engine

---

## 🏗️ Proposed Directory Structure

```
src/                          # GENERIC ENGINE (reusable for any game)
  components/                 # Generic components
  systems/                    # Generic systems
  core/                       # Engine core
  rendering/                  # Rendering engine
  spatial/                    # Spatial partitioning
  io/                         # Asset loading
  utils/                      # Utilities
  materials/                  # Material system
  config/                     # Engine configuration
  debug/                      # Debug tools (ECS Inspector)

game/                         # POKEMON-SPECIFIC GAME CODE
  pokemon/                    # Pokemon data and logic
    PokemonSpecies.h          # (New - Phase 8.2)
    PokemonCreature.h         # (New - Phase 8.2)
    PokemonParty.h            # (New - Phase 8.2)
  systems/                    # Pokemon-specific systems
    CreatureDetectionSystem.h # (Moved from src/game/)
    GrassSystem.h             # (Moved from src/game/)
  managers/                   # Pokemon game managers
    PokemonSpawner.h          # (New - Phase 8.2)
    PokemonCaptureManager.h   # (New - Phase 8.2)
  ui/                         # Pokemon UI
    PartyUI.h                 # (New - Phase 8.2)

examples/                     # EXAMPLE CODE (not part of engine/game)
  ECS_Example.cpp             # (Moved from src/examples/)
```

---

## 📋 Refactoring Tasks

### **Task 1: Create game/ Directory Structure**

```bash
mkdir -p game/pokemon
mkdir -p game/systems
mkdir -p game/managers
mkdir -p game/ui
```

**Deliverable**: Directory structure ready for game code

---

### **Task 2: Move CreatureDetectionSystem to game/**

#### Step 2.1: Move File
```bash
git mv src/game/CreatureDetectionSystem.h game/systems/CreatureDetectionSystem.h
```

#### Step 2.2: Update Include Paths in File
File: `game/systems/CreatureDetectionSystem.h`

Change includes from:
```cpp
#include "../core/SystemImpl.h"
#include "../components/Transform.h"
```

To:
```cpp
#include "../../src/core/SystemImpl.h"
#include "../../src/components/Transform.h"
```

#### Step 2.3: Update Files That Include CreatureDetectionSystem

**Files to Update**:
- `src/core/Application.h`
  ```cpp
  // OLD:
  #include "../game/CreatureDetectionSystem.h"

  // NEW:
  #include "../../game/systems/CreatureDetectionSystem.h"
  ```

- `src/core/ApplicationSetup.cpp` (same change)
- `tests_cpp/performance/test_CreatureDetectionRegression.cpp`
  ```cpp
  // OLD:
  #include "../src/game/CreatureDetectionSystem.h"

  // NEW:
  #include "../../game/systems/CreatureDetectionSystem.h"
  ```

**Deliverable**: CreatureDetectionSystem in correct location

---

### **Task 3: Move GrassSystem to game/**

#### Step 3.1: Move File
```bash
git mv src/game/GrassSystem.h game/systems/GrassSystem.h
```

#### Step 3.2: Update Include Paths
Same pattern as Task 2

**Note**: Check if GrassSystem is currently used in Application. If not used, we can defer integration.

**Deliverable**: GrassSystem in correct location

---

### **Task 4: Delete src/game/ Directory**

```bash
rmdir src/game  # Should be empty after moves
```

**Deliverable**: No more `src/game/` confusion

---

### **Task 5: Move Examples to Top-Level**

#### Step 5.1: Create examples/ Directory
```bash
mkdir -p examples
```

#### Step 5.2: Move Example Files
```bash
git mv src/examples/ECS_Example.cpp examples/ECS_Example.cpp
```

#### Step 5.3: Delete src/examples/
```bash
rmdir src/examples
```

**Deliverable**: Examples separated from engine code

---

### **Task 6: Update CMakeLists.txt**

#### File: `CMakeLists.txt` (main)

**Current**: May reference `src/game/` or `src/examples/`

**Action**:
- Remove any references to `src/game/`
- Remove any references to `src/examples/`
- Game code will have its own CMakeLists.txt later

**Deliverable**: Engine builds without game dependencies

---

### **Task 7: Update Test CMakeLists.txt**

#### File: `tests_cpp/CMakeLists.txt`

**Update include paths**:
```cmake
# Add game directory to include path
target_include_directories(vulkanmon_tests PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${Stb_INCLUDE_DIR}
    fixtures/
    ../game  # NEW: Add game directory
)
```

**Note**: Tests can include game code since they're testing the full application

**Deliverable**: Tests still build and pass

---

### **Task 8: Verify Build After Refactoring**

```bash
cd build
cmake --build . 2>&1 | tee refactor_build.log
```

**Expected**: Clean build with no errors

**If errors**: Fix include paths in missed files

**Deliverable**: Project builds successfully

---

### **Task 9: Run Full Test Suite**

```bash
cd build/tests_cpp
Debug/vulkanmon_tests.exe
```

**Expected**: All 1993 assertions pass

**If failures**: Fix test include paths

**Deliverable**: All tests pass

---

### **Task 10: Update CLAUDE.md Documentation**

#### File: `CLAUDE.md`

**Add section**:
```markdown
### Directory Structure

**Engine Code** (`src/`):
- Generic, reusable engine systems
- No game-specific logic
- Can be used for any game type

**Game Code** (`game/`):
- Pokemon-specific gameplay logic
- Uses engine components and systems
- Contains Pokemon rules and mechanics

**Examples** (`examples/`):
- Example code and tutorials
- Not included in builds
```

**Deliverable**: Documentation reflects new structure

---

## 🧪 Validation Checklist

After completing all tasks:

- [ ] `src/game/` directory deleted
- [ ] `src/examples/` directory deleted
- [ ] `game/systems/CreatureDetectionSystem.h` exists
- [ ] `game/systems/GrassSystem.h` exists
- [ ] `examples/ECS_Example.cpp` exists
- [ ] Project builds cleanly (zero warnings preferred)
- [ ] All 1993+ test assertions pass
- [ ] No broken include paths
- [ ] Git history preserved with `git mv`

---

## 🔍 Critical Analysis: What Needs Refactoring

### **CreatureDetectionSystem - Keep or Refactor?**

**Current State**: Pokemon-specific with `CreatureComponent`, `CreatureState`, creature AI

**Question**: Is this generic enough for engine, or Pokemon-specific?

**Answer**: **Pokemon-specific** - Move to `game/systems/`

**Reasoning**:
- `CreatureState` enum names (WANDERING, FLEEING, AGGRESSIVE) are Pokemon-specific
- Detection radii and behavior are tuned for Pokemon gameplay
- References "player" entity implicitly

**Alternative**: Could create generic `AIBehaviorComponent` in engine, then `CreatureComponent` extends it in game layer

**Recommendation**:
- Move as-is to `game/systems/` (quick win)
- In Phase 8.2.2, create generic `AIBehaviorComponent` in `src/components/`
- Keep `CreatureDetectionSystem` as Pokemon-specific wrapper

---

### **GrassSystem - Keep or Refactor?**

**Current State**: Pokemon-specific grass encounter system

**Question**: Generic or Pokemon-specific?

**Answer**: **100% Pokemon-specific** - Move to `game/systems/`

**Reasoning**:
- Entire system is about Pokemon grass encounters
- Spawn rates, grass types are Pokemon mechanics
- No other game would use this exact system

**Recommendation**: Move to `game/systems/`, keep as reference but don't integrate into Phase 8.2 (not needed for MVP)

---

## ⏱️ Estimated Time

**Total Refactoring Time**: 1-2 hours

**Breakdown**:
- Tasks 1-5 (File moves): 30 minutes
- Tasks 6-7 (CMake updates): 15 minutes
- Task 8 (Build verification): 15 minutes
- Task 9 (Test verification): 15 minutes
- Task 10 (Documentation): 15 minutes

**Risk**: Low - mostly file moves with include path updates

---

## 🚦 Recommendation: Do This First

**Priority**: HIGH

**Why**:
1. **Prevents Confusion**: Clear engine/game boundary before adding more code
2. **Clean Foundation**: Phase 8.2 code goes in right place from start
3. **Low Risk**: Just moving files, not changing logic
4. **Fast**: 1-2 hours of work

**When**: Before starting Phase 8.2.1 (Player Controller)

**Alternative**: Skip refactoring, put new game code in `game/` but leave old files in `src/game/`
- **Pro**: Faster start on gameplay
- **Con**: Mixed organization, confusing for future development

---

## 📝 Git Commit Strategy

```bash
# Commit 1: Create directory structure
git add game/ examples/
git commit -m "Refactor: Create game/ and examples/ directories for code organization"

# Commit 2: Move CreatureDetectionSystem
git mv src/game/CreatureDetectionSystem.h game/systems/CreatureDetectionSystem.h
# ... update includes ...
git commit -m "Refactor: Move CreatureDetectionSystem to game/systems/"

# Commit 3: Move GrassSystem
git mv src/game/GrassSystem.h game/systems/GrassSystem.h
# ... update includes ...
git commit -m "Refactor: Move GrassSystem to game/systems/"

# Commit 4: Move examples
git mv src/examples/ECS_Example.cpp examples/ECS_Example.cpp
git commit -m "Refactor: Move examples to top-level examples/ directory"

# Commit 5: Cleanup
rmdir src/game src/examples
git commit -m "Refactor: Remove old src/game and src/examples directories"

# Commit 6: Build system updates
# ... CMakeLists.txt changes ...
git commit -m "Refactor: Update build system for new directory structure"

# Commit 7: Documentation
git commit -m "Docs: Update CLAUDE.md with new directory structure"
```

---

## ✅ Success Criteria

**Refactoring is complete when**:
1. No files exist in `src/game/` or `src/examples/`
2. `game/` directory contains Pokemon-specific code
3. `examples/` directory contains example code
4. Project builds with zero errors
5. All tests pass (1993+ assertions)
6. Documentation reflects new structure
7. Git history preserved with proper `git mv` commands

---

*This refactoring establishes clean architecture before Phase 8.2 gameplay implementation, following the "Simple is Powerful" philosophy.*
