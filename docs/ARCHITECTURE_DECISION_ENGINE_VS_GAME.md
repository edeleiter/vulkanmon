# Architecture Decision: Engine vs Game Code Organization

## 🎯 Decision: Modern C++ Project Layout

**Date**: September 30, 2025
**Status**: APPROVED
**Principle**: "Simple is Powerful" - Follow industry-standard C++ conventions

---

## 📁 Final Directory Structure

```
vulkanmon/
├── src/                          # ALL C++ SOURCE CODE
│   ├── components/               # Generic engine components
│   │   ├── Transform.h
│   │   ├── Renderable.h
│   │   └── Camera.h
│   │
│   ├── systems/                  # Generic engine systems
│   │   ├── RenderSystem.h/cpp
│   │   ├── PhysicsSystem.h/cpp
│   │   └── CameraSystem.h/cpp
│   │
│   ├── game/                     # POKEMON-SPECIFIC C++ CODE
│   │   ├── CreatureDetectionSystem.h      # Pokemon creature AI
│   │   ├── GrassSystem.h                  # Pokemon grass encounters
│   │   ├── PokemonParty.h/cpp            # (Phase 8.2)
│   │   ├── PokemonSpawner.h/cpp          # (Phase 8.2)
│   │   └── PokemonCaptureManager.h/cpp   # (Phase 8.2)
│   │
│   ├── core/                     # Engine core
│   ├── rendering/                # Rendering engine
│   ├── spatial/                  # Spatial partitioning
│   └── utils/                    # Utilities
│
├── game/                         # POKEMON GAME DATA/ASSETS (Future)
│   ├── data/
│   │   ├── pokemon_species.yaml  # Pokemon definitions
│   │   └── move_database.json    # Move data
│   └── config/
│       └── game_balance.json     # Balance tweaks
│
├── assets/                       # ENGINE ASSETS
│   ├── models/                   # 3D models
│   ├── textures/                 # Textures
│   └── shaders/                  # GLSL shaders
│
└── examples/                     # EXAMPLE/TUTORIAL CODE
    └── ECS_Example.cpp
```

---

## 🔍 Key Principles

### **1. All C++ Code Lives in `src/`**
**Why**: Standard C++ convention - source files belong in source directory

**Rule**: If it's a `.h` or `.cpp` file, it goes in `src/` (including game-specific code)

**Rationale**:
- ✅ Follows CMake/C++ best practices
- ✅ Compile performance (proper .h/.cpp separation)
- ✅ IDE tooling works better (IntelliSense, etc.)
- ✅ Matches Unity/Unreal structure

### **2. `src/game/` Contains Pokemon-Specific C++ Systems**
**What goes here**: C++ code that only makes sense for Pokemon gameplay

**Examples**:
- `CreatureDetectionSystem` - Pokemon creature AI behavior
- `GrassSystem` - Pokemon grass encounter mechanics
- `PokemonParty` - Pokemon party management
- `PokemonCaptureManager` - Pokeball capture logic

**What does NOT go here**: Generic systems that could be reused

### **3. `game/` (Top-Level) for Data/Config Files (Future)**
**What goes here**: Non-code game assets and data

**Examples**:
- Pokemon species definitions (YAML/JSON)
- Move database
- Item definitions
- Game balance configuration
- Level/map data

**Why separate**:
- Data files are NOT compiled
- Can be hot-reloaded without recompilation
- Game designers can edit without touching C++

### **4. `src/components/` and `src/systems/` are Generic**
**Rule**: If another game could use it, it's generic engine code

**Generic Components** (go in `src/components/`):
- `Transform` - All games need transforms
- `Renderable` - All 3D games need rendering
- `RigidBodyComponent` - Generic physics
- `AIBehaviorComponent` - Generic AI (Phase 8.2)
- `CharacterControllerComponent` - Generic character controller

**Pokemon Components** (go in `src/game/`):
- `CreatureComponent` - Pokemon-specific creature data
- `PokemonCreature` - Pokemon instance
- `GrassComponent` - Pokemon grass patches

---

## 📊 Decision Matrix

| Type | Location | Example | Rationale |
|------|----------|---------|-----------|
| Generic Engine .h/.cpp | `src/systems/` | `RenderSystem.cpp` | Reusable for any game |
| Pokemon C++ Code | `src/game/` | `PokemonParty.cpp` | Pokemon-specific logic |
| Engine Assets | `assets/` | `sphere.obj` | Generic 3D models |
| Pokemon Data Files | `game/data/` | `pokemon_species.yaml` | Game data (future) |
| Example Code | `examples/` | `ECS_Example.cpp` | Not part of build |

---

## 🏗️ Why This is Better Than Alternative Approaches

### ❌ **Rejected: Separate game/ with C++ code**
```
game/
  systems/
    PokemonParty.cpp  ← C++ code outside src/
```

**Why rejected**:
- Violates C++ conventions (.cpp outside src/)
- Confuses CMake build system
- Poor compile performance (harder to split .h/.cpp)
- Doesn't match industry standards

### ❌ **Rejected: Everything in src/ with no game/ subdirectory**
```
src/
  systems/
    RenderSystem.cpp
    PokemonParty.cpp  ← Mixed generic/game systems
```

**Why rejected**:
- No clear separation between engine and game
- Can't easily extract engine for reuse
- Confusing which systems are Pokemon-specific

### ✅ **Chosen: src/game/ for Pokemon C++ code**
```
src/
  systems/          ← Generic
  game/            ← Pokemon C++
    PokemonParty.cpp
```

**Why chosen**:
- ✅ Clear separation while following C++ conventions
- ✅ All .cpp files in src/ as expected
- ✅ Easy to identify Pokemon-specific code
- ✅ Can extract `src/systems/`, `src/components/` as generic engine
- ✅ Matches industry patterns (Unity, Unreal, Godot)

---

## 🎯 Phase 8.2 Implementation Guidelines

When implementing Phase 8.2 (Playable Demo), follow these rules:

### **Generic Engine Components → `src/components/`**
```cpp
src/components/CharacterControllerComponent.h
src/components/AIBehaviorComponent.h
src/components/CaptureComponent.h
```

### **Generic Engine Systems → `src/systems/`**
```cpp
src/systems/CharacterControllerSystem.h
src/systems/CharacterControllerSystem.cpp  // Split when implementation gets large
src/systems/AIBehaviorSystem.h
src/systems/AIBehaviorSystem.cpp
src/systems/CaptureSystem.h
src/systems/CaptureSystem.cpp
```

### **Pokemon-Specific Code → `src/game/`**
```cpp
src/game/PokemonSpecies.h          // Pokemon data structures
src/game/PokemonCreature.h         // Pokemon instance
src/game/PokemonParty.h
src/game/PokemonParty.cpp
src/game/PokemonSpawner.h
src/game/PokemonSpawner.cpp
src/game/PokemonCaptureManager.h   // Pokemon-specific capture logic
src/game/PokemonCaptureManager.cpp
```

### **When to Split .h and .cpp**

**Keep Header-Only** when:
- Component is pure data (< 50 lines)
- Template-heavy implementation
- Tiny helper functions

**Split .h/.cpp** when:
- Implementation > 100 lines
- Complex business logic
- Want to reduce compile times
- Multiple translation units use it

---

## 📝 CMake Integration

### **Main CMakeLists.txt**
```cmake
# Engine source files
set(ENGINE_SOURCES
    src/core/Application.cpp
    src/systems/RenderSystem.cpp
    # ... other engine files
)

# Game-specific source files
set(GAME_SOURCES
    src/game/PokemonParty.cpp
    src/game/PokemonSpawner.cpp
    src/game/PokemonCaptureManager.cpp
    # ... other game files
)

add_executable(vulkanmon
    src/main.cpp
    ${ENGINE_SOURCES}
    ${GAME_SOURCES}
)
```

**Benefit**: Clear separation in build system, can easily exclude game code if extracting engine

---

## 🎮 Future: Game Data Files (Phase 9+)

Once we need data-driven Pokemon definitions:

```
game/
  data/
    pokemon_species.yaml:
      - id: 1
        name: Bulbasaur
        type: [Grass, Poison]
        baseStats: {hp: 45, attack: 49, ...}
```

**Benefits**:
- Game designers edit YAML, not C++
- Hot-reload balance changes without recompiling
- Modding support (users can add Pokemon)
- Clear separation: `src/game/` = code, `game/data/` = data

---

## ✅ Summary

**Decision**: Use `src/game/` for Pokemon-specific C++ code

**Rationale**:
1. Follows C++ conventions (all .cpp in src/)
2. Clear engine/game separation via subdirectory
3. Industry-standard layout
4. Better compile performance
5. "Simple is Powerful" - don't fight language conventions

**Next Steps**:
- Keep existing `src/game/CreatureDetectionSystem.h` where it is
- Add Phase 8.2 game code to `src/game/`
- Create `game/data/` later when needed for YAML/JSON files

---

*This architecture decision follows VulkanMon's "Simple is Powerful" philosophy by adhering to established C++ project conventions rather than inventing custom structures.*
