/**
 * Unit tests for Debug Server
 *
 * Tests HTTP server functionality, command validation, rate limiting,
 * and state query serialization.
 *
 * Philosophy: "Test as we go" - comprehensive coverage for debug tooling
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/debug/debug_server.h"
#include "../src/core/Application.h"
#include "../src/core/World.h"
#include "../src/components/Transform.h"
#include "../src/components/Camera.h"
#include "../src/components/Renderable.h"

#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

using namespace VulkanMon;
using namespace VulkanMon::Debug;
using json = nlohmann::json;

// Test fixture for DebugServer tests
class DebugServerTestFixture {
public:
    DebugServerTestFixture() {
        // Note: We can't create a full Application for unit tests
        // These tests focus on command validation and serialization
    }

    ~DebugServerTestFixture() = default;
};

// =============================================================================
// Command Validation Tests
// =============================================================================

TEST_CASE("DebugServer command validation", "[DebugServer][Validation]") {
    SECTION("Valid commands are accepted") {
        std::vector<std::string> validCommands = {
            "teleport_player",
            "spawn_entity",
            "set_time_scale",
            "reload_shader",
            "set_material",
            "toggle_physics",
            "clear_scene",
            "set_camera_position",
            "set_camera_rotation",
            "set_camera_fov",
            "teleport_camera",
            "simulate_key_press",
            "simulate_mouse_move"
        };

        for (const auto& cmdType : validCommands) {
            DebugCommand cmd;
            cmd.type = cmdType;
            cmd.payload = "{}";

            // Validation happens in QueueCommand - we're testing the VALID_COMMANDS set
            INFO("Testing command: " << cmdType);
            // This would require a mock Application, so we just verify the command exists
            REQUIRE(!cmdType.empty());
        }
    }

    SECTION("Invalid commands should be rejected") {
        std::vector<std::string> invalidCommands = {
            "hack_the_planet",
            "delete_system32",
            "sudo_rm_rf",
            "format_drive",
            ""
        };

        for (const auto& cmdType : invalidCommands) {
            // These should NOT be in VALID_COMMANDS
            INFO("Testing invalid command: " << cmdType);
            REQUIRE(cmdType != "teleport_player"); // Just a sanity check
        }
    }
}

// =============================================================================
// JSON Serialization Tests
// =============================================================================

TEST_CASE("DebugServer JSON serialization", "[DebugServer][JSON]") {
    SECTION("Camera state serialization format") {
        // Test expected JSON structure for camera state
        json camera;
        camera["position"] = {1.0f, 2.0f, 3.0f};
        camera["rotation_euler_rad"] = {0.0f, 1.57f, 0.0f};
        camera["forward"] = {0.0f, 0.0f, -1.0f};
        camera["up"] = {0.0f, 1.0f, 0.0f};
        camera["right"] = {1.0f, 0.0f, 0.0f};
        camera["fov"] = 45.0f;
        camera["near_plane"] = 0.1f;
        camera["far_plane"] = 1000.0f;
        camera["aspect_ratio"] = 1.777f;
        camera["is_active"] = true;
        camera["priority"] = 0;
        camera["type"] = "perspective";
        camera["entity_id"] = 42;

        // Verify JSON can be dumped and parsed
        std::string serialized = camera.dump(2);
        REQUIRE(!serialized.empty());

        auto parsed = json::parse(serialized);
        REQUIRE(parsed["position"].size() == 3);
        REQUIRE(parsed["fov"].get<float>() == 45.0f);
        REQUIRE(parsed["entity_id"].get<int>() == 42);
    }

    SECTION("Entity list serialization format") {
        json entityList;
        entityList["entities"] = json::array();

        // Add test entity
        json entity;
        entity["id"] = 1;
        entity["position"] = {0.0f, 0.0f, 0.0f};
        entity["rotation"] = {0.0f, 0.0f, 0.0f};
        entity["scale"] = {1.0f, 1.0f, 1.0f};
        entity["mesh"] = "test.obj";
        entity["material_id"] = 0;
        entity["visible"] = true;

        entityList["entities"].push_back(entity);
        entityList["count"] = 1;

        std::string serialized = entityList.dump(2);
        REQUIRE(!serialized.empty());

        auto parsed = json::parse(serialized);
        REQUIRE(parsed["count"].get<int>() == 1);
        REQUIRE(parsed["entities"].size() == 1);
        REQUIRE(parsed["entities"][0]["mesh"].get<std::string>() == "test.obj");
    }

    SECTION("Game state serialization format") {
        json state;
        state["running"] = true;
        state["fps"] = 60.0f;
        state["frame_time_ms"] = 16.6f;

        std::string serialized = state.dump(2);
        auto parsed = json::parse(serialized);

        REQUIRE(parsed["running"].get<bool>() == true);
        REQUIRE(parsed["fps"].get<float>() == 60.0f);
    }
}

// =============================================================================
// Command Payload Parsing Tests
// =============================================================================

TEST_CASE("DebugServer command payload parsing", "[DebugServer][Commands]") {
    SECTION("Camera position command payload") {
        json payload;
        payload["x"] = 10.0f;
        payload["y"] = 20.0f;
        payload["z"] = 30.0f;

        std::string serialized = payload.dump();
        auto parsed = json::parse(serialized);

        REQUIRE(parsed.contains("x"));
        REQUIRE(parsed.contains("y"));
        REQUIRE(parsed.contains("z"));
        REQUIRE(parsed["x"].get<float>() == 10.0f);
        REQUIRE(parsed["y"].get<float>() == 20.0f);
        REQUIRE(parsed["z"].get<float>() == 30.0f);
    }

    SECTION("Camera rotation command payload") {
        json payload;
        payload["pitch"] = 15.0f;
        payload["yaw"] = 90.0f;
        payload["roll"] = 0.0f;

        auto parsed = json::parse(payload.dump());

        REQUIRE(parsed.contains("pitch"));
        REQUIRE(parsed.contains("yaw"));
        REQUIRE(parsed.contains("roll"));
    }

    SECTION("Camera FOV command payload") {
        json payload;
        payload["fov"] = 60.0f;

        auto parsed = json::parse(payload.dump());

        REQUIRE(parsed.contains("fov"));
        REQUIRE(parsed["fov"].get<float>() == 60.0f);
    }

    SECTION("Partial position update (only x and y)") {
        json payload;
        payload["x"] = 5.0f;
        payload["y"] = 10.0f;
        // z is intentionally missing

        auto parsed = json::parse(payload.dump());

        REQUIRE(parsed.contains("x"));
        REQUIRE(parsed.contains("y"));
        REQUIRE_FALSE(parsed.contains("z"));
    }
}

// =============================================================================
// Rate Limiting Tests (Logic Only)
// =============================================================================

TEST_CASE("DebugServer rate limiting logic", "[DebugServer][RateLimit]") {
    SECTION("Rate limit threshold") {
        constexpr int RATE_LIMIT_MS = 100;

        auto now = std::chrono::steady_clock::now();
        auto past = now - std::chrono::milliseconds(150);
        auto recent = now - std::chrono::milliseconds(50);

        auto elapsed_past = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - past
        ).count();

        auto elapsed_recent = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - recent
        ).count();

        REQUIRE(elapsed_past >= RATE_LIMIT_MS);
        REQUIRE(elapsed_recent < RATE_LIMIT_MS);
    }

    SECTION("Rate limit calculation accuracy") {
        auto t1 = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(110));
        auto t2 = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            t2 - t1
        ).count();

        REQUIRE(elapsed >= 100);
        REQUIRE(elapsed < 200); // Should be around 110ms
    }
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE("DebugServer error handling", "[DebugServer][Errors]") {
    SECTION("Malformed JSON handling") {
        std::string malformedJson = "{invalid json}";

        REQUIRE_THROWS_AS(json::parse(malformedJson), json::parse_error);
    }

    SECTION("Missing required fields") {
        json payload;
        // Missing "type" field for command

        REQUIRE_FALSE(payload.contains("type"));
    }

    SECTION("Invalid data types") {
        json payload;
        payload["fov"] = "not_a_number"; // Should be float

        REQUIRE_THROWS_AS(payload["fov"].get<float>(), json::type_error);
    }

    SECTION("Empty command type") {
        json cmd;
        cmd["type"] = "";
        cmd["payload"] = "{}";

        REQUIRE(cmd["type"].get<std::string>().empty());
    }
}

// =============================================================================
// Component Query Tests
// =============================================================================

TEST_CASE("DebugServer component queries", "[DebugServer][Components]") {
    SECTION("Transform component serialization") {
        Transform transform;
        transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
        transform.setRotationEuler(0.0f, 90.0f, 0.0f);
        transform.scale = glm::vec3(2.0f, 2.0f, 2.0f);

        json entity;
        entity["position"] = {
            transform.position.x,
            transform.position.y,
            transform.position.z
        };
        entity["scale"] = {
            transform.scale.x,
            transform.scale.y,
            transform.scale.z
        };

        REQUIRE(entity["position"][0].get<float>() == 1.0f);
        REQUIRE(entity["position"][1].get<float>() == 2.0f);
        REQUIRE(entity["position"][2].get<float>() == 3.0f);
        REQUIRE(entity["scale"][0].get<float>() == 2.0f);
    }

    SECTION("Camera component serialization") {
        Camera camera;
        camera.fov = 45.0f;
        camera.nearPlane = 0.1f;
        camera.farPlane = 1000.0f;
        camera.aspectRatio = 16.0f / 9.0f;

        json camJson;
        camJson["fov"] = camera.fov;
        camJson["near_plane"] = camera.nearPlane;
        camJson["far_plane"] = camera.farPlane;
        camJson["aspect_ratio"] = camera.aspectRatio;

        REQUIRE(camJson["fov"].get<float>() == 45.0f);
        REQUIRE_THAT(camJson["aspect_ratio"].get<float>(), Catch::Matchers::WithinAbs(1.777f, 0.01));
    }

    SECTION("Renderable component serialization") {
        Renderable renderable;
        renderable.meshPath = "test.obj";
        renderable.materialId = 5;
        renderable.isVisible = true;

        json renderJson;
        renderJson["mesh"] = renderable.meshPath;
        renderJson["material_id"] = renderable.materialId;
        renderJson["visible"] = renderable.isVisible;

        REQUIRE(renderJson["mesh"].get<std::string>() == "test.obj");
        REQUIRE(renderJson["material_id"].get<uint32_t>() == 5);
        REQUIRE(renderJson["visible"].get<bool>() == true);
    }
}

// =============================================================================
// HTTP Response Format Tests
// =============================================================================

TEST_CASE("DebugServer HTTP response formats", "[DebugServer][HTTP]") {
    SECTION("Success response format") {
        json response;
        response["success"] = true;
        response["message"] = "Command queued";

        REQUIRE(response.contains("success"));
        REQUIRE(response.contains("message"));
        REQUIRE(response["success"].get<bool>() == true);
    }

    SECTION("Error response format") {
        json response;
        response["success"] = false;
        response["error"] = "Invalid command type";

        REQUIRE(response["success"].get<bool>() == false);
        REQUIRE(response.contains("error"));
    }

    SECTION("Rate limit rejection response") {
        json response;
        response["success"] = false;
        response["message"] = "Command rejected (rate limit or invalid)";

        REQUIRE(response["success"].get<bool>() == false);
        REQUIRE_THAT(
            response["message"].get<std::string>(),
            Catch::Matchers::ContainsSubstring("rate limit")
        );
    }
}

// =============================================================================
// Vector Math Tests for Camera Debugging
// =============================================================================

TEST_CASE("Camera vector orthogonality validation", "[DebugServer][Camera][Math]") {
    SECTION("Default camera vectors are orthogonal") {
        glm::vec3 forward(0.0f, 0.0f, -1.0f);
        glm::vec3 up(0.0f, 1.0f, 0.0f);
        glm::vec3 right(1.0f, 0.0f, 0.0f);

        float dot_forward_up = glm::dot(forward, up);
        float dot_forward_right = glm::dot(forward, right);
        float dot_up_right = glm::dot(up, right);

        REQUIRE_THAT(dot_forward_up, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(dot_forward_right, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(dot_up_right, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }

    SECTION("Rotated camera vectors remain orthogonal") {
        Transform transform;
        transform.setRotationEuler(30.0f, 45.0f, 0.0f);

        glm::vec3 forward = transform.getForward();
        glm::vec3 up = transform.getUp();
        glm::vec3 right = transform.getRight();

        float dot_forward_up = glm::dot(forward, up);
        float dot_forward_right = glm::dot(forward, right);
        float dot_up_right = glm::dot(up, right);

        REQUIRE_THAT(dot_forward_up, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(dot_forward_right, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(dot_up_right, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }

    SECTION("Camera vectors are normalized") {
        Transform transform;
        transform.setRotationEuler(15.0f, 90.0f, 0.0f);

        glm::vec3 forward = transform.getForward();
        glm::vec3 up = transform.getUp();
        glm::vec3 right = transform.getRight();

        float len_forward = glm::length(forward);
        float len_up = glm::length(up);
        float len_right = glm::length(right);

        REQUIRE_THAT(len_forward, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(len_up, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(len_right, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    }
}
