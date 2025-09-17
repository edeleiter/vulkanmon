#pragma once

namespace VulkanMon::Config {
    /**
     * Unified camera configuration - Single source of truth
     * Used by: VulkanRenderer, Application, ECS Camera components
     *
     * This replaces scattered constants across:
     * - Application.h (DEFAULT_* constants)
     * - VulkanRenderer.cpp (hardcoded CAMERA_FOV, FAR_PLANE, etc.)
     * - Components/Camera.h (default member values)
     */
    struct Camera {
        // =================================================================
        // Projection Settings
        // =================================================================

        /// Field of view in degrees - wider FOV shows more creatures
        static constexpr float DEFAULT_FOV = 75.0f;

        /// Near clipping plane distance
        static constexpr float DEFAULT_NEAR_PLANE = 0.1f;

        /// Far clipping plane distance - extended for Pokemon-style open world
        /// CRITICAL: Must be > 25.0f for current camera position at Z=25
        static constexpr float DEFAULT_FAR_PLANE = 200.0f;

        // =================================================================
        // Movement Settings
        // =================================================================

        /// Camera movement speed for WASD controls
        static constexpr float DEFAULT_SPEED = 2.5f;

        // =================================================================
        // Display Settings
        // =================================================================

        /// Default aspect ratio for projection matrix
        static constexpr float DEFAULT_ASPECT_RATIO = 16.0f / 9.0f;

        /// Default window width
        static constexpr int DEFAULT_WINDOW_WIDTH = 800;

        /// Default window height
        static constexpr int DEFAULT_WINDOW_HEIGHT = 600;

        // =================================================================
        // Performance Settings
        // =================================================================

        /// Maximum render distance for distance-based culling
        static constexpr float DEFAULT_MAX_RENDER_DISTANCE = 1000.0f;

        /// Default update rate for camera systems (FPS)
        static constexpr float DEFAULT_UPDATE_RATE = 60.0f;
    };

    /**
     * Future configuration structs can follow this pattern:
     * - Config::Render for rendering settings
     * - Config::Input for control settings
     * - Config::Performance for optimization thresholds
     * - Config::Debug for logging and debug settings
     */
}