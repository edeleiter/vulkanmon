#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    // Depth test demo: First triangle closer (-0.5), second triangle farther (-0.8)
    float depth = (gl_VertexIndex < 3) ? -0.5 : -0.8;
    gl_Position = ubo.model * vec4(inPosition, depth, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}