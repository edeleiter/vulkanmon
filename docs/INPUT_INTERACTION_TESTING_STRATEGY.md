# Input & Interaction Testing Strategy

**Purpose:** Prevent functionality regressions during engine refactoring, especially for user-facing features like material switching, lighting controls, and camera movement.

## Current Problem Analysis

### Material Switching (M Key) Regression
**Root Cause:** During multi-descriptor set refactor, material switching logic likely still references old unified descriptor set approach instead of new per-material descriptor sets.

**Impact:** User interaction fails silently - button press registered but no visual change occurs.

**Why This Wasn't Caught:** No automated tests for input->rendering pipeline integration.

## Comprehensive Testing Strategy

### 1. Input System Unit Tests
```cpp
// Test input registration and callback triggering
TEST_CASE("Input System - Material Control Registration") {
    InputHandler inputHandler;
    bool materialCallbackTriggered = false;

    inputHandler.registerMaterialCallback([&](uint32_t materialId) {
        materialCallbackTriggered = true;
    });

    inputHandler.processKey(GLFW_KEY_M, GLFW_PRESS);
    REQUIRE(materialCallbackTriggered == true);
}

// Test hot reload functionality
TEST_CASE("Input System - Hot Reload Callback") {
    InputHandler inputHandler;
    bool reloadTriggered = false;

    inputHandler.registerShaderReloadCallback([&]() {
        reloadTriggered = true;
    });

    inputHandler.processKey(GLFW_KEY_R, GLFW_PRESS);
    REQUIRE(reloadTriggered == true);
}
```

### 2. Material System Integration Tests
```cpp
// Test material cycling functionality
TEST_CASE("Material System - Cycling Through Materials") {
    auto materialSystem = createTestMaterialSystem();

    // Create test materials
    MaterialData defaultMat, goldMat, rubyMat;
    uint32_t mat1 = materialSystem->createMaterial(defaultMat);
    uint32_t mat2 = materialSystem->createMaterial(goldMat);
    uint32_t mat3 = materialSystem->createMaterial(rubyMat);

    // Test cycling logic
    uint32_t currentMaterial = 0;
    MaterialCycler cycler(materialSystem.get());

    REQUIRE(cycler.getNextMaterial(currentMaterial) == 1);
    REQUIRE(cycler.getNextMaterial(1) == 2);
    REQUIRE(cycler.getNextMaterial(2) == 0); // Wrap around
}

// Test descriptor set binding
TEST_CASE("Material System - Descriptor Set Binding") {
    auto materialSystem = createTestMaterialSystem();
    MaterialData testMaterial;
    uint32_t materialId = materialSystem->createMaterial(testMaterial);

    VkDescriptorSet descriptorSet = materialSystem->getDescriptorSet(materialId);
    REQUIRE(descriptorSet != VK_NULL_HANDLE);
}
```

### 3. End-to-End Interaction Tests
```cpp
// Test complete material switching pipeline
TEST_CASE("E2E - Material Switching Pipeline") {
    MockVulkanRenderer renderer;
    MockInputHandler inputHandler;
    MockMaterialSystem materialSystem;

    // Setup scene with test object
    EntityID testEntity = setupTestEntityWithMaterial(0);

    // Simulate M key press
    inputHandler.simulateKeyPress(GLFW_KEY_M);

    // Verify material ID changed in ECS
    auto& renderable = world.getComponent<Renderable>(testEntity);
    REQUIRE(renderable.materialId == 1); // Should increment

    // Verify descriptor set binding called
    REQUIRE(renderer.wasDescriptorSetBound(1, materialSystem.getDescriptorSet(1)));
}

// Test lighting control pipeline
TEST_CASE("E2E - Lighting Control Pipeline") {
    MockLightingSystem lightingSystem;
    MockInputHandler inputHandler;

    float initialIntensity = lightingSystem.getDirectionalIntensity();

    inputHandler.simulateKeyPress(GLFW_KEY_1); // Increase intensity

    REQUIRE(lightingSystem.getDirectionalIntensity() > initialIntensity);
}
```

### 4. Automated Functional Testing Framework
```cpp
class FunctionalTestFramework {
public:
    void runInteractionTest(const std::string& testName,
                           std::function<void()> setup,
                           std::function<void()> interaction,
                           std::function<bool()> verification) {
        SECTION(testName) {
            setup();
            interaction();
            REQUIRE(verification());
        }
    }
};

// Usage:
TEST_CASE("Functional - All Material Controls") {
    FunctionalTestFramework framework;

    framework.runInteractionTest("Material Cycling",
        []() { setupSceneWithMultipleMaterials(); },
        []() { simulateKeyPress(GLFW_KEY_M); },
        []() { return getCurrentMaterialId() != getInitialMaterialId(); }
    );

    framework.runInteractionTest("Shininess Control",
        []() { setupMaterialWithShininess(32.0f); },
        []() { simulateKeyPress(GLFW_KEY_5); },
        []() { return getMaterialShininess() < 32.0f; }
    );
}
```

### 5. Visual/Rendering Validation Tests
```cpp
// Test that material changes result in visual differences
TEST_CASE("Visual - Material Changes Affect Rendering") {
    TestRenderTarget renderTarget;
    VulkanRenderer renderer;

    // Render with material 0
    renderer.setCurrentMaterial(0);
    auto frame1 = renderer.captureFrame(renderTarget);

    // Render with material 1
    renderer.setCurrentMaterial(1);
    auto frame2 = renderer.captureFrame(renderTarget);

    // Verify frames are different (materials have visual impact)
    REQUIRE(compareFrames(frame1, frame2) > MINIMUM_VISUAL_DIFFERENCE);
}
```

## Implementation Recommendations

### 1. Test-Driven Development for Interactions
**Before** adding new input features:
1. Write tests for expected behavior
2. Implement the feature to make tests pass
3. Refactor while keeping tests green

### 2. Continuous Integration Checks
```yaml
# CI Pipeline addition
- name: Run Interaction Tests
  run: |
    cd build/tests_cpp
    ./vulkanmon_tests --tags="[Input]"
    ./vulkanmon_tests --tags="[Material]"
    ./vulkanmon_tests --tags="[E2E]"
```

### 3. Mock System Architecture
```cpp
// Create testable abstractions
class IMaterialSystem {
public:
    virtual uint32_t createMaterial(const MaterialData& data) = 0;
    virtual VkDescriptorSet getDescriptorSet(uint32_t id) = 0;
    virtual size_t getMaterialCount() = 0;
};

class MockMaterialSystem : public IMaterialSystem {
    // Controlled testing implementation
};
```

### 4. Input System Validation
```cpp
class InputValidator {
public:
    static void validateAllControlsWork() {
        // Programmatically test every registered input
        for (auto& [key, callback] : registeredCallbacks) {
            REQUIRE_NOTHROW(callback());
        }
    }
};
```

### 5. Regression Detection System
```cpp
// Automated regression detection
class InteractionRegessionDetector {
public:
    void recordBaseline() {
        // Record current behavior for all inputs
        baselineBehaviors = captureAllInteractionBehaviors();
    }

    void detectRegressions() {
        auto currentBehaviors = captureAllInteractionBehaviors();
        auto differences = compare(baselineBehaviors, currentBehaviors);

        if (!differences.empty()) {
            logRegressions(differences);
            FAIL("Interaction regressions detected");
        }
    }
};
```

## Immediate Action Plan

### 1. Fix Current Material Switching Issue
**Root Cause:** Material switching logic needs update for new descriptor set architecture
**Location:** Likely in material callback registration or rendering update logic

### 2. Implement Core Interaction Tests
**Priority 1:** Material cycling (M key)
**Priority 2:** Lighting controls (1/2/3/4/L keys)
**Priority 3:** Hot reload (R key)

### 3. Add CI Integration
- Run interaction tests on every PR
- Fail builds if user-facing features break
- Generate interaction test reports

### 4. Documentation Requirements
- Document all input mappings
- Require test updates for new interactions
- Maintain interaction behavior specifications

## Benefits of This Approach

### 1. **Regression Prevention**
- Catch breaking changes before they reach users
- Ensure refactoring doesn't break existing functionality
- Maintain confidence during large architectural changes

### 2. **Documentation Through Tests**
- Tests serve as executable documentation
- Clear specifications of expected behavior
- Easy onboarding for new developers

### 3. **Rapid Development Feedback**
- Quick identification of broken interactions
- Automated verification of fixes
- Confidence in making changes

### 4. **Professional Development Standards**
- Industry-standard testing practices
- Suitable for serious game development
- Foundation for larger team collaboration

This strategy ensures that user-facing features remain functional during engine evolution, preventing the "M key not working" type of regressions that can significantly impact development workflow.