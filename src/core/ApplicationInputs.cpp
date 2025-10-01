#include "Application.h"
#include "../utils/Logger.h"
#include <stdexcept>

using namespace VulkanMon;

// =============================================================================
// Input Handling Methods
// =============================================================================

void Application::handleShaderReload() {
    VKMON_INFO("Reloading shaders...");

    try {
        if (renderer_) {
            renderer_->reloadShaders();
            VKMON_INFO("Hot reload complete - new shaders active!");
        }
    } catch (const std::exception& e) {
        VKMON_ERROR("Hot reload failed: " + std::string(e.what()));
        VKMON_WARNING("Application continuing with previous shaders");
    }
}

void Application::handleLightingControl(int key) {
    // Lighting system integration points
    switch (key) {
        case GLFW_KEY_1:  // Decrease directional light intensity
            adjustDirectionalLightIntensity(-0.1f);
            break;
        case GLFW_KEY_2:  // Increase directional light intensity
            adjustDirectionalLightIntensity(0.1f);
            break;
        case GLFW_KEY_3:  // Cycle lighting presets
            cycleLightingPreset();
            break;
        case GLFW_KEY_4:  // Toggle ambient lighting
            toggleAmbientLighting();
            break;
        case GLFW_KEY_L:  // Print lighting debug info
            printLightingInfo();
            break;
        case GLFW_KEY_7:  // Test raycast system
            testRaycastSystem();
            break;
        case GLFW_KEY_8:  // Test sphere overlap queries
            testSphereOverlapQueries();
            break;
        case GLFW_KEY_9:  // Measure spatial collision performance
            measureSpatialCollisionPerformance();
            break;
        case GLFW_KEY_0:  // Run comprehensive physics validation tests
            runPhysicsValidationTests();
            break;
        default:
            // Unknown lighting control key
            break;
    }
}

void Application::handleMaterialControl(int key) {
    // Material system integration points
    switch (key) {
        case GLFW_KEY_M:  // Cycle material presets
            cycleMaterialPreset();
            break;
        case GLFW_KEY_5:  // Decrease material shininess
            adjustMaterialShininess(-10.0f);
            break;
        case GLFW_KEY_6:  // Increase material shininess
            adjustMaterialShininess(10.0f);
            break;
        default:
            // Unknown material control key
            break;
    }
}

// =============================================================================
// Lighting Control Methods
// =============================================================================

void Application::adjustDirectionalLightIntensity(float delta) {
    if (lightingSystem_) {
        auto& lighting = lightingSystem_->getCurrentLighting();
        lighting.directionalLight.intensity += delta;

        // Clamp intensity to reasonable range
        if (lighting.directionalLight.intensity < 0.0f) lighting.directionalLight.intensity = 0.0f;
        if (lighting.directionalLight.intensity > 3.0f) lighting.directionalLight.intensity = 3.0f;

        lightingSystem_->updateLighting(lighting);
        VKMON_INFO("[LIGHTING] Directional light intensity: " + std::to_string(lighting.directionalLight.intensity));
    } else {
        VKMON_WARNING("[LIGHTING] LightingSystem not available for intensity adjustment");
    }
}

void Application::cycleLightingPreset() {
    if (lightingSystem_) {
        lightingState_.currentPreset = (lightingState_.currentPreset + 1) % 4;
        auto& lighting = lightingSystem_->getCurrentLighting();

        // Apply different lighting presets
        const char* presetNames[] = {"Default", "Daylight", "Sunset", "Moonlight"};

        switch (lightingState_.currentPreset) {
            case 0: // Default
                lighting.directionalLight.direction = glm::normalize(glm::vec3(0.0f, -1.0f, -0.3f));
                lighting.directionalLight.intensity = 1.0f;
                lighting.directionalLight.color = glm::vec3(1.0f, 1.0f, 1.0f);
                lighting.ambientColor = glm::vec3(0.2f, 0.2f, 0.3f);
                lighting.ambientIntensity = 0.3f;
                break;
            case 1: // Daylight
                lighting.directionalLight.direction = glm::normalize(glm::vec3(0.3f, -1.0f, -0.2f));
                lighting.directionalLight.intensity = 1.2f;
                lighting.directionalLight.color = glm::vec3(1.0f, 0.95f, 0.8f);
                lighting.ambientColor = glm::vec3(0.4f, 0.4f, 0.5f);
                lighting.ambientIntensity = 0.4f;
                break;
            case 2: // Sunset
                lighting.directionalLight.direction = glm::normalize(glm::vec3(0.8f, -0.3f, -0.5f));
                lighting.directionalLight.intensity = 0.8f;
                lighting.directionalLight.color = glm::vec3(1.0f, 0.6f, 0.3f);
                lighting.ambientColor = glm::vec3(0.3f, 0.2f, 0.3f);
                lighting.ambientIntensity = 0.2f;
                break;
            case 3: // Moonlight
                lighting.directionalLight.direction = glm::normalize(glm::vec3(-0.2f, -1.0f, 0.3f));
                lighting.directionalLight.intensity = 0.5f;
                lighting.directionalLight.color = glm::vec3(0.8f, 0.9f, 1.0f);
                lighting.ambientColor = glm::vec3(0.1f, 0.1f, 0.2f);
                lighting.ambientIntensity = 0.1f;
                break;
        }

        lightingSystem_->updateLighting(lighting);
        VKMON_INFO("[LIGHTING] Cycled to preset: " + std::string(presetNames[lightingState_.currentPreset]));
    } else {
        VKMON_WARNING("[LIGHTING] LightingSystem not available for preset cycling");
    }
}

void Application::toggleAmbientLighting() {
    if (lightingSystem_) {
        lightingState_.ambientEnabled = !lightingState_.ambientEnabled;
        auto& lighting = lightingSystem_->getCurrentLighting();

        if (lightingState_.ambientEnabled) {
            lighting.ambientIntensity = 0.3f;  // Restore ambient
        } else {
            lighting.ambientIntensity = 0.0f;  // Disable ambient
        }

        lightingSystem_->updateLighting(lighting);
        VKMON_INFO("[LIGHTING] Ambient lighting: " + std::string(lightingState_.ambientEnabled ? "enabled" : "disabled"));
    } else {
        VKMON_WARNING("[LIGHTING] LightingSystem not available for ambient toggle");
    }
}

void Application::printLightingInfo() {
    VKMON_INFO("=== Lighting Debug Info ===");
    if (lightingSystem_) {
        const auto& lighting = lightingSystem_->getCurrentLighting();
        const char* presetNames[] = {"Default", "Daylight", "Sunset", "Moonlight"};

        VKMON_INFO("Current preset: " + std::string(presetNames[lightingState_.currentPreset]));
        VKMON_INFO("Directional Light:");
        VKMON_INFO("  Direction: (" + std::to_string(lighting.directionalLight.direction.x) +
                   ", " + std::to_string(lighting.directionalLight.direction.y) +
                   ", " + std::to_string(lighting.directionalLight.direction.z) + ")");
        VKMON_INFO("  Intensity: " + std::to_string(lighting.directionalLight.intensity));
        VKMON_INFO("  Color: (" + std::to_string(lighting.directionalLight.color.x) +
                   ", " + std::to_string(lighting.directionalLight.color.y) +
                   ", " + std::to_string(lighting.directionalLight.color.z) + ")");
        VKMON_INFO("Ambient Light:");
        VKMON_INFO("  Intensity: " + std::to_string(lighting.ambientIntensity));
        VKMON_INFO("  Color: (" + std::to_string(lighting.ambientColor.x) +
                   ", " + std::to_string(lighting.ambientColor.y) +
                   ", " + std::to_string(lighting.ambientColor.z) + ")");
    } else {
        VKMON_INFO("LightingSystem not available");
    }
    VKMON_INFO("===========================");
}

// =============================================================================
// Material Control Methods
// =============================================================================

void Application::adjustMaterialShininess(float delta) {
    materialState_.shininess += delta;

    // Clamp shininess to reasonable range
    if (materialState_.shininess < 1.0f) materialState_.shininess = 1.0f;
    if (materialState_.shininess > 256.0f) materialState_.shininess = 256.0f;

    VKMON_INFO("[MATERIAL] Shininess adjusted to: " + std::to_string(materialState_.shininess));

    // Future: Update material system with new shininess value
}

void Application::cycleMaterialPreset() {
    if (renderer_) {
        renderer_->cycleMaterialPreset();
    }

    // Keep local state in sync for any future needs
    materialState_.currentPreset = (materialState_.currentPreset + 1) % 5;
}

// =============================================================================
// Error Handling
// =============================================================================

void Application::handleCriticalError(const std::exception& error) {
    VKMON_ERROR("Critical application error: " + std::string(error.what()));

    // Log additional system state for debugging
    logSystemState();

    // Attempt graceful shutdown
    shutdown();
}

void Application::logSystemState() {
    VKMON_INFO("=== System State Debug Info ===");
    VKMON_INFO("Application initialized: " + std::string(initialized_ ? "true" : "false"));
    VKMON_INFO("Application running: " + std::string(running_ ? "true" : "false"));
    VKMON_INFO("Frame time: " + std::to_string(frameTime_) + "ms");
    VKMON_INFO("FPS: " + std::to_string(fps_));

    if (renderer_) {
        VKMON_INFO("Renderer frame time: " + std::to_string(renderer_->getFrameTime()) + "ms");
    } else {
        VKMON_INFO("Renderer: " + std::string(renderer_ ? "not initialized" : "null"));
    }

    VKMON_INFO("===============================");
}