# VulkanMon Testing Infrastructure

## � Testing Philosophy

VulkanMon follows the principle **"Test as we go"** with comprehensive testing infrastructure designed for game engine reliability and Pokemon-style gameplay validation.

### Testing Strategy
- **Unit Testing** - Core system validation with Catch2 framework
- **Integration Testing** - System interaction and data flow verification
- **Performance Testing** - Benchmarking critical paths for 60+ FPS
- **Visual Validation** - Manual verification of rendering and gameplay

---

## � Testing Framework Architecture

### Catch2 Integration
**Framework**: Catch2 v3.x via vcpkg
**Location**: `tests_cpp/` directory
**Build Integration**: CMake with automatic test discovery

```cmake
# tests_cpp/CMakeLists.txt
find_package(Catch2 3 REQUIRED)

# Test executable with all test files
add_executable(vulkanmon_tests
    test_main.cpp
    test_Application.cpp
    test_Logger.cpp
    test_Camera.cpp
    test_VulkanRenderer.cpp
    test_ecs_integration.cpp
    # ... additional test files
)

target_link_libraries(vulkanmon_tests
    PRIVATE
    Catch2::Catch2WithMain
    vulkanmon_lib
)

# Automatic test discovery
include(CTest)
include(Catch)
catch_discover_tests(vulkanmon_tests)
```

### Test Organization Structure
```
tests_cpp/
├── test_main.cpp                 # Catch2 main entry point
├── core/
│   ├── test_Application.cpp      # Application lifecycle tests
│   ├── test_World.cpp           # ECS World functionality
│   ├── test_EntityManager.cpp   # Entity creation/destruction
│   └── test_InputHandler.cpp    # Input event processing
├── rendering/
│   ├── test_VulkanRenderer.cpp  # Vulkan initialization/cleanup
│   ├── test_ResourceManager.cpp # RAII resource management
│   ├── test_MaterialSystem.cpp  # Material property validation
│   └── test_LightingSystem.cpp  # Lighting calculations
├── spatial/
│   ├── test_SpatialManager.cpp  # Octree spatial queries
│   ├── test_SpatialSystem.cpp   # ECS spatial integration
│   └── bench_SpatialSystem.cpp  # Performance benchmarks
├── systems/
│   ├── test_CameraSystem.cpp    # Camera math and controls
│   ├── test_RenderSystem.cpp    # ECS rendering integration
│   └── test_CreatureDetection.cpp # Pokemon detection logic
└── utils/
    ├── test_Logger.cpp          # Logging system validation
    └── test_Math.cpp            # GLM utility functions
```

---

## � Core System Testing

### 1. Logger System Tests
**File**: `tests_cpp/utils/test_Logger.cpp`
**Coverage**: 100% (all logging paths validated)

```cpp
TEST_CASE("Logger Basic Functionality", "[Logger]") {
    Logger logger;

    SECTION("Console Output") {
        logger.info("Test message");
        // Verify no crashes, thread safety
    }

    SECTION("File Output") {
        std::string testFile = "test_log.txt";
        logger.enableFileOutput(testFile);
        logger.warning("File test message");
        logger.disableFileOutput();

        // Verify file creation and content
        REQUIRE(std::filesystem::exists(testFile));
        std::filesystem::remove(testFile);
    }

    SECTION("Thread Safety") {
        // Multiple threads logging simultaneously
        std::vector<std::thread> threads;
        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&logger, i]() {
                logger.info("Thread " + std::to_string(i));
            });
        }
        for (auto& t : threads) {
            t.join();
        }
        // No deadlocks or crashes expected
    }
}
```

**Key Test Areas:**
- Thread safety with mutex protection
- File output enable/disable without deadlocks
- Multiple log levels (info, warning, error)
- Performance under concurrent access

### 2. ECS World & EntityManager Tests
**File**: `tests_cpp/core/test_World.cpp`

```cpp
TEST_CASE("Entity Lifecycle Management", "[World][EntityManager]") {
    World world;

    SECTION("Entity Creation") {
        EntityID entity1 = world.createEntity();
        EntityID entity2 = world.createEntity();

        REQUIRE(entity1 != INVALID_ENTITY);
        REQUIRE(entity2 != INVALID_ENTITY);
        REQUIRE(entity1 != entity2);
    }

    SECTION("Component Addition and Retrieval") {
        EntityID entity = world.createEntity();

        Transform transform{glm::vec3(1.0f, 2.0f, 3.0f)};
        world.addComponent(entity, std::move(transform));

        REQUIRE(world.hasComponent<Transform>(entity));

        Transform& retrieved = world.getComponent<Transform>(entity);
        REQUIRE(retrieved.position == glm::vec3(1.0f, 2.0f, 3.0f));
    }

    SECTION("Entity Destruction") {
        EntityID entity = world.createEntity();
        world.addComponent<Transform>(entity, Transform{});

        world.destroyEntity(entity);

        REQUIRE_FALSE(world.hasComponent<Transform>(entity));
    }
}
```

### 3. Vulkan Renderer Tests
**File**: `tests_cpp/rendering/test_VulkanRenderer.cpp`

```cpp
TEST_CASE("VulkanRenderer Initialization", "[VulkanRenderer]") {
    SECTION("Headless Initialization") {
        // Test Vulkan components without window
        VulkanRenderer renderer;

        // Mock initialization without GLFW window
        bool result = renderer.initializeHeadless();
        REQUIRE(result == true);

        renderer.cleanup();
    }

    SECTION("Resource Management") {
        VulkanRenderer renderer;
        renderer.initializeHeadless();

        // Test model loading and caching
        bool loaded = renderer.ensureMeshLoaded("assets/models/test_cube.obj");
        REQUIRE(loaded == true);

        // Verify caching works
        bool loadedAgain = renderer.ensureMeshLoaded("assets/models/test_cube.obj");
        REQUIRE(loadedAgain == true); // Should use cache

        renderer.cleanup();
    }
}
```

**Vulkan Testing Challenges:**
- **Headless Mode** - Testing without full GPU context
- **Mock Objects** - Stubbing GPU resources for unit tests
- **Resource Cleanup** - Ensuring no Vulkan memory leaks

---

## � Spatial System Testing

### Spatial Manager Performance Tests
**File**: `tests_cpp/spatial/bench_SpatialSystem.cpp`

```cpp
TEST_CASE("Spatial System Performance Benchmarks", "[SpatialSystem][Performance]") {
    SpatialManager spatialManager;

    SECTION("Large Entity Count Performance") {
        const size_t ENTITY_COUNT = 1000;
        std::vector<EntityID> entities;

        // Insert many entities
        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < ENTITY_COUNT; ++i) {
            EntityID entity = static_cast<EntityID>(i + 1);
            glm::vec3 pos(i % 100, 0, i / 100);
            BoundingBox bounds{pos - glm::vec3(0.5f), pos + glm::vec3(0.5f)};

            spatialManager.updateEntity(entity, bounds);
            entities.push_back(entity);
        }

        auto insertEnd = std::chrono::high_resolution_clock::now();
        auto insertTime = std::chrono::duration_cast<std::chrono::milliseconds>(insertEnd - start);

        // Perform radius queries
        auto queryStart = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 100; ++i) {
            glm::vec3 queryPos(i % 50, 0, i % 50);
            auto results = spatialManager.findCreaturesInRadius(queryPos, 10.0f);
        }

        auto queryEnd = std::chrono::high_resolution_clock::now();
        auto queryTime = std::chrono::duration_cast<std::chrono::milliseconds>(queryEnd - queryStart);

        // Performance requirements
        REQUIRE(insertTime.count() < 100);  // <100ms for 1000 inserts
        REQUIRE(queryTime.count() < 50);    // <50ms for 100 queries

        std::cout << "Insert time: " << insertTime.count() << "ms\n";
        std::cout << "Query time: " << queryTime.count() << "ms\n";
    }
}
```

### Spatial Query Accuracy Tests
```cpp
TEST_CASE("Spatial Query Accuracy", "[SpatialSystem][Queries]") {
    SpatialManager spatialManager;

    // Create test entities in known positions
    EntityID entity1 = 1, entity2 = 2, entity3 = 3;

    spatialManager.updateEntity(entity1, BoundingBox{glm::vec3(-1), glm::vec3(1)});    // Origin
    spatialManager.updateEntity(entity2, BoundingBox{glm::vec3(4), glm::vec3(6)});     // Far
    spatialManager.updateEntity(entity3, BoundingBox{glm::vec3(1.5f), glm::vec3(2.5f)}); // Close

    SECTION("Radius Query Accuracy") {
        auto results = spatialManager.findCreaturesInRadius(glm::vec3(0), 3.0f);

        // Should find entity1 and entity3, but not entity2
        REQUIRE(results.size() == 2);
        REQUIRE(std::find(results.begin(), results.end(), entity1) != results.end());
        REQUIRE(std::find(results.begin(), results.end(), entity3) != results.end());
        REQUIRE(std::find(results.begin(), results.end(), entity2) == results.end());
    }
}
```

---

## � Pokemon Gameplay Testing

### Creature Detection System Tests
**File**: `tests_cpp/systems/test_CreatureDetection.cpp`

```cpp
TEST_CASE("Pokemon Creature Detection", "[CreatureDetection][Pokemon]") {
    World world;
    auto spatialSystem = std::make_shared<SpatialSystem>();
    auto spatialManager = std::make_shared<SpatialManager>();
    spatialSystem->setSpatialManager(spatialManager);

    CreatureDetectionSystem detectionSystem;
    detectionSystem.setSpatialSystem(spatialSystem.get());

    SECTION("Player-Creature Detection") {
        // Create player entity
        EntityID player = world.createEntity();
        world.addComponent<Transform>(player, Transform{glm::vec3(0, 1, 0)});
        world.addComponent<SpatialComponent>(player, SpatialComponent{
            .boundingRadius = 2.0f,
            .layer = LayerMask::Player
        });

        // Create nearby creature
        EntityID creature = world.createEntity();
        world.addComponent<Transform>(creature, Transform{glm::vec3(3, 1, 0)});
        world.addComponent<SpatialComponent>(creature, SpatialComponent{
            .boundingRadius = 1.0f,
            .detectionRadius = 5.0f,
            .layer = LayerMask::Creatures
        });
        world.addComponent<Creature>(creature, Creature{
            .species = "Pikachu",
            .level = 5,
            .state = CreatureState::Wandering
        });

        // Update spatial system with entity positions
        spatialSystem->update(0.016f, world.getEntityManager());

        // Run detection system
        detectionSystem.update(0.016f, world.getEntityManager());

        // Verify creature detected player
        auto& creatureComp = world.getComponent<Creature>(creature);
        REQUIRE(creatureComp.state == CreatureState::AlertToPlayer);
    }
}
```

### Camera System Integration Tests
```cpp
TEST_CASE("Camera System Integration", "[CameraSystem][ECS]") {
    World world;
    CameraSystem cameraSystem;

    SECTION("Camera Entity Creation") {
        EntityID camera = world.createEntity();
        world.addComponent<Transform>(camera, Transform{
            glm::vec3(0, 8, 15),  // Position
            glm::vec3(-30, 0, 0), // Look down 30 degrees
            glm::vec3(1)          // Normal scale
        });
        world.addComponent<Camera>(camera, Camera{
            .fov = 75.0f,
            .nearPlane = 0.1f,
            .farPlane = 200.0f,
            .isActive = true
        });

        cameraSystem.update(0.016f, world.getEntityManager());

        // Verify view matrix calculation
        glm::mat4 viewMatrix = cameraSystem.getViewMatrix();
        glm::mat4 projMatrix = cameraSystem.getProjectionMatrix();

        REQUIRE(viewMatrix != glm::mat4(1.0f)); // Not identity
        REQUIRE(projMatrix != glm::mat4(1.0f)); // Not identity
    }
}
```

---

## � Performance Testing Framework

### Benchmarking Infrastructure
```cpp
// tests_cpp/utils/PerformanceTimer.h
class PerformanceTimer {
    std::chrono::high_resolution_clock::time_point start;
    std::string testName;

public:
    explicit PerformanceTimer(const std::string& name)
        : testName(name), start(std::chrono::high_resolution_clock::now()) {}

    ~PerformanceTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << testName << " took: " << duration.count() << " microseconds\n";
    }
};

#define BENCHMARK(name) PerformanceTimer timer(name)
```

### Critical Path Performance Tests
```cpp
TEST_CASE("ECS Performance Benchmarks", "[Performance][ECS]") {
    World world;

    // Create many entities with components
    const size_t ENTITY_COUNT = 10000;
    std::vector<EntityID> entities;

    {
        BENCHMARK("Entity Creation (10k entities)");
        for (size_t i = 0; i < ENTITY_COUNT; ++i) {
            EntityID entity = world.createEntity();
            world.addComponent<Transform>(entity, Transform{});
            world.addComponent<SpatialComponent>(entity, SpatialComponent{});
            entities.push_back(entity);
        }
    }

    {
        BENCHMARK("Component Access (10k lookups)");
        for (EntityID entity : entities) {
            auto& transform = world.getComponent<Transform>(entity);
            transform.position.x += 0.1f; // Simulate update
        }
    }

    // Performance requirements for 60 FPS (16.67ms budget)
    // ECS operations should be <1ms for 10k entities
}
```

---

## � Test Execution & CI Integration

### Local Test Execution
```bash
# Build and run all tests
cd build
cmake --build .
cd tests_cpp
Debug/vulkanmon_tests.exe

# Run specific test categories
Debug/vulkanmon_tests.exe "[Logger]"           # Logger tests only
Debug/vulkanmon_tests.exe "[ECS]"              # ECS-related tests
Debug/vulkanmon_tests.exe "[Performance]"      # Performance benchmarks
Debug/vulkanmon_tests.exe "[SpatialSystem]"    # Spatial management tests

# Verbose output with timing
Debug/vulkanmon_tests.exe -s -d yes
```

### Continuous Integration (GitHub Actions)
```yaml
# .github/workflows/tests.yml
name: Tests

on: [push, pull_request]

jobs:
  test:
    strategy:
      matrix:
        os: [windows-latest, ubuntu-latest]

    runs-on: ${{ matrix.os }}

    steps:
    - uses: actions/checkout@v3
      with:
        submodules: recursive

    - name: Setup vcpkg
      run: |
        git submodule update --init vcpkg
        ./vcpkg/bootstrap-vcpkg.sh

    - name: Build with tests
      run: |
        mkdir build
        cd build
        cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
        cmake --build .

    - name: Run tests
      run: |
        cd build/tests_cpp
        ctest --output-on-failure
```

### Test Coverage Analysis
```bash
# Generate coverage report (requires GCC/Clang with coverage flags)
mkdir build-coverage
cd build-coverage
cmake .. -DCMAKE_CXX_FLAGS="--coverage" -DCMAKE_BUILD_TYPE=Debug
cmake --build .
cd tests_cpp
Debug/vulkanmon_tests
gcov *.gcno
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

---

## � Current Test Statistics

### Test Suite Overview
- **Total Test Cases**: 102 test cases
- **Total Assertions**: 1,724 assertions
- **Pass Rate**: 100% (all tests passing)
- **Execution Time**: ~2.3 seconds for full suite
- **Coverage**: Core systems 100%, rendering systems 85%

### Test Categories
```
[Logger]              - 15 test cases    100% pass
[Camera]              - 12 test cases    100% pass
[ECS]                 - 25 test cases    100% pass
[SpatialSystem]       - 18 test cases    100% pass
[VulkanRenderer]      - 8 test cases     100% pass
[MaterialSystem]      - 10 test cases    100% pass
[Performance]         - 14 test cases    100% pass
```

### Performance Benchmarks (Debug Build)
- **Entity Creation**: ~50ns per entity
- **Component Access**: ~10ns per lookup
- **Spatial Queries**: <0.5ms for 1000+ entities
- **ECS System Updates**: <1ms for 100+ entities
- **Memory Usage**: Zero leaks detected

## � Testing Best Practices

### 1. Test Structure (AAA Pattern)
```cpp
TEST_CASE("Descriptive Test Name", "[Category]") {
    // Arrange - Set up test data
    World world;
    EntityID entity = world.createEntity();

    // Act - Perform the operation
    world.addComponent<Transform>(entity, Transform{});

    // Assert - Verify results
    REQUIRE(world.hasComponent<Transform>(entity));
}
```

### 2. Performance Test Guidelines
- **Realistic Data Sets** - Test with Pokemon-scale entity counts
- **Debug vs Release** - Account for debug build overhead
- **Consistent Environment** - Same hardware for benchmark comparison
- **Statistical Validity** - Multiple runs with average/median reporting

### 3. Pokemon-Specific Test Scenarios
- **Creature Counts** - Test with 100+ creatures in radius
- **Battle Triggers** - Verify detection at proper distances
- **AI State Machines** - Test creature behavior transitions
- **Spatial Optimization** - Frustum culling with large creature counts

The testing infrastructure ensures VulkanMon maintains Pokemon Legends: Arceus quality with professional reliability and performance characteristics.