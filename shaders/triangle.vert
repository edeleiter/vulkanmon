#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    // 3D Cube depth assignment: Front face closer, back face farther
    float depth;
    if (gl_VertexIndex < 4) {
        depth = 0.5;  // Front face (vertices 0-3)
    } else {
        depth = -0.5; // Back face (vertices 4-7)
    }
    
    // Complete MVP transformation: Projection * View * Model
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, depth, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}