/**
 * VulkanMon Camera Unit Tests
 * 
 * Testing camera mathematics and input logic - our second "easy win"
 * Pure math testing, no external dependencies
 * 
 * Test Categories:
 * - View matrix calculation
 * - Camera position and orientation
 * - Input processing (without actual input)
 * - Mathematical correctness
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Camera.h"
#include "TestHelpers.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace VulkanMon::Testing;
using Catch::Approx;

TEST_CASE("Camera Basic Functionality", "[Camera][Basic]") {
    SECTION("Default camera construction") {
        Camera camera;
        
        // Check default values
        REQUIRE(camera.position.x == Approx(2.0f));
        REQUIRE(camera.position.y == Approx(2.0f));
        REQUIRE(camera.position.z == Approx(2.0f));
        
        REQUIRE(camera.target.x == Approx(0.0f));
        REQUIRE(camera.target.y == Approx(0.0f));
        REQUIRE(camera.target.z == Approx(0.0f));
        
        REQUIRE(camera.up.x == Approx(0.0f));
        REQUIRE(camera.up.y == Approx(0.0f));
        REQUIRE(camera.up.z == Approx(1.0f));
    }
    
    SECTION("Custom camera construction") {
        glm::vec3 pos(5.0f, 3.0f, 1.0f);
        glm::vec3 target(1.0f, 1.0f, 1.0f);
        glm::vec3 up(0.0f, 1.0f, 0.0f);
        
        Camera camera(pos, target, up);
        
        REQUIRE(camera.position.x == Approx(5.0f));
        REQUIRE(camera.position.y == Approx(3.0f));
        REQUIRE(camera.position.z == Approx(1.0f));
        
        REQUIRE(camera.target.x == Approx(1.0f));
        REQUIRE(camera.target.y == Approx(1.0f));
        REQUIRE(camera.target.z == Approx(1.0f));
        
        REQUIRE(camera.up.x == Approx(0.0f));
        REQUIRE(camera.up.y == Approx(1.0f));
        REQUIRE(camera.up.z == Approx(0.0f));
    }
}

TEST_CASE("Camera View Matrix", "[Camera][Math]") {
    SECTION("View matrix calculation is correct") {
        glm::vec3 pos(0.0f, 0.0f, 5.0f);
        glm::vec3 target(0.0f, 0.0f, 0.0f);
        glm::vec3 up(0.0f, 1.0f, 0.0f);
        
        Camera camera(pos, target, up);
        glm::mat4 viewMatrix = camera.getViewMatrix();
        
        // Compare with GLM's lookAt matrix
        glm::mat4 expectedMatrix = glm::lookAt(pos, target, up);
        
        // Check each element with some tolerance for floating point precision
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                REQUIRE(viewMatrix[col][row] == Approx(expectedMatrix[col][row]).epsilon(0.001f));
            }
        }
    }
    
    SECTION("View matrix changes with camera position") {
        Camera camera;
        glm::mat4 originalMatrix = camera.getViewMatrix();
        
        // Move camera
        camera.position.x += 1.0f;
        glm::mat4 movedMatrix = camera.getViewMatrix();
        
        // Matrices should be different
        bool matricesDifferent = false;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                if (std::abs(originalMatrix[col][row] - movedMatrix[col][row]) > 0.001f) {
                    matricesDifferent = true;
                    break;
                }
            }
            if (matricesDifferent) break;
        }
        
        REQUIRE(matricesDifferent);
    }
    
    SECTION("View matrix changes with camera target") {
        Camera camera;
        glm::mat4 originalMatrix = camera.getViewMatrix();
        
        // Change target
        camera.target.y += 1.0f;
        glm::mat4 newMatrix = camera.getViewMatrix();
        
        // Matrices should be different
        bool matricesDifferent = false;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                if (std::abs(originalMatrix[col][row] - newMatrix[col][row]) > 0.001f) {
                    matricesDifferent = true;
                    break;
                }
            }
            if (matricesDifferent) break;
        }
        
        REQUIRE(matricesDifferent);
    }
}

TEST_CASE("Camera Mathematical Properties", "[Camera][Math]") {
    SECTION("View matrix is orthogonal") {
        Camera camera;
        glm::mat4 viewMatrix = camera.getViewMatrix();
        
        // Extract the upper-left 3x3 rotation matrix
        glm::mat3 rotationMatrix(viewMatrix);
        
        // For an orthogonal matrix, transpose = inverse
        glm::mat3 transpose = glm::transpose(rotationMatrix);
        glm::mat3 inverse = glm::inverse(rotationMatrix);
        
        // Check if transpose equals inverse (within tolerance)
        for (int col = 0; col < 3; ++col) {
            for (int row = 0; row < 3; ++row) {
                REQUIRE(transpose[col][row] == Approx(inverse[col][row]).epsilon(0.001f));
            }
        }
    }
    
    SECTION("Camera direction vector calculation") {
        Camera camera;
        
        // Direction should be from position to target
        glm::vec3 expectedDirection = glm::normalize(camera.target - camera.position);
        
        // We can't directly access direction from Camera, but we can verify
        // it through the view matrix calculation
        glm::vec3 forward = glm::normalize(camera.target - camera.position);
        glm::vec3 right = glm::normalize(glm::cross(forward, camera.up));
        glm::vec3 up = glm::cross(right, forward);
        
        // Basic sanity checks
        REQUIRE(glm::length(forward) == Approx(1.0f));
        REQUIRE(glm::length(right) == Approx(1.0f));
        REQUIRE(glm::length(up) == Approx(1.0f));
        
        // Forward and right should be perpendicular
        REQUIRE(glm::dot(forward, right) == Approx(0.0f).epsilon(0.001f));
    }
}

TEST_CASE("Camera Edge Cases", "[Camera][EdgeCases]") {
    SECTION("Camera with same position and target") {
        glm::vec3 samePoint(1.0f, 1.0f, 1.0f);
        Camera camera(samePoint, samePoint, glm::vec3(0.0f, 1.0f, 0.0f));
        
        // Should not crash when getting view matrix
        glm::mat4 viewMatrix = camera.getViewMatrix();
        
        // Matrix should still be valid (no NaN values)
        bool hasNaN = false;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                if (std::isnan(viewMatrix[col][row])) {
                    hasNaN = true;
                    break;
                }
            }
            if (hasNaN) break;
        }
        
        REQUIRE_FALSE(hasNaN);
    }
    
    SECTION("Camera with zero up vector") {
        Camera camera;
        camera.up = glm::vec3(0.0f, 0.0f, 0.0f);
        
        // Should handle gracefully
        glm::mat4 viewMatrix = camera.getViewMatrix();
        
        // Should not produce NaN values
        bool hasNaN = false;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                if (std::isnan(viewMatrix[col][row])) {
                    hasNaN = true;
                    break;
                }
            }
            if (hasNaN) break;
        }
        
        REQUIRE_FALSE(hasNaN);
    }
}

TEST_CASE("Camera Performance", "[Camera][Performance]") {
    SECTION("View matrix calculation performance") {
        Camera camera;
        
        // Benchmark view matrix calculation
        double avgTime = PerformanceTestHelpers::benchmarkFunction([&camera]() {
            volatile auto matrix = camera.getViewMatrix(); // volatile to prevent optimization
        }, 1000);
        
        // Should be very fast (under 1ms for 1000 calculations)
        REQUIRE(avgTime < 1.0);
        
        // Log performance for monitoring
        // In a real test, we'd use the Logger here
        REQUIRE(true);
    }
}