#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(binding = 2) uniform LightingData {
    vec3 directionalLightDir;
    float directionalLightIntensity;
    vec3 directionalLightColor;
    float _padding1;
    vec3 ambientColor;
    float ambientIntensity;
} lighting;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inColor;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 worldPos;

void main() {
    // Calculate world position for lighting calculations
    vec4 worldPosition = ubo.model * vec4(inPosition, 1.0);
    worldPos = worldPosition.xyz;
    
    // Complete MVP transformation: Projection * View * Model
    gl_Position = ubo.proj * ubo.view * worldPosition;
    
    // Transform normal to world space (using normal matrix)
    fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;
    
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}