#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

struct GLFWwindow;

class Camera {
public:
    Camera(glm::vec3 pos = glm::vec3(2.0f, 2.0f, 2.0f), 
           glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f), 
           glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f));
    
    void processInput(GLFWwindow* window);
    glm::mat4 getViewMatrix() const;
    
    glm::vec3 position;
    glm::vec3 target;
    glm::vec3 up;

private:
    std::chrono::high_resolution_clock::time_point lastFrameTime;
};