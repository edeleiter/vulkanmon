#version 450

// Uniform buffer for per-frame data (view, projection, camera)
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float _padding;
} ubo;

layout(set = 0, binding = 2) uniform LightingData {
    vec3 directionalLightDir;
    float directionalLightIntensity;
    vec3 directionalLightColor;
    float _padding1;
    vec3 ambientColor;
    float ambientIntensity;
} lighting;

// Per-vertex input attributes (binding 0)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inColor;

// Per-instance input attributes (binding 1) - GPU instancing data
layout(location = 4) in vec4 instanceModelMatrix0;  // First row of model matrix
layout(location = 5) in vec4 instanceModelMatrix1;  // Second row of model matrix
layout(location = 6) in vec4 instanceModelMatrix2;  // Third row of model matrix
layout(location = 7) in vec4 instanceModelMatrix3;  // Fourth row of model matrix
layout(location = 8) in vec4 instanceMaterialParams; // Material ID + custom params
layout(location = 9) in vec4 instanceLodParams;     // LOD level + distance + custom

// Output to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 worldPos;
layout(location = 4) out vec3 cameraPos;
layout(location = 5) out float materialId;

void main() {
    // Reconstruct the model matrix from per-instance data
    mat4 instanceModelMatrix = mat4(
        instanceModelMatrix0,
        instanceModelMatrix1,
        instanceModelMatrix2,
        instanceModelMatrix3
    );

    // Calculate world position using instance-specific model matrix
    vec4 worldPosition = instanceModelMatrix * vec4(inPosition, 1.0);
    worldPos = worldPosition.xyz;

    // Complete MVP transformation: Projection * View * Model (instanced)
    gl_Position = ubo.proj * ubo.view * worldPosition;

    // Transform normal to world space using instance model matrix
    fragNormal = mat3(transpose(inverse(instanceModelMatrix))) * inNormal;

    // Pass through vertex attributes
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    cameraPos = ubo.cameraPos;

    // Pass instance material ID to fragment shader
    materialId = instanceMaterialParams.x;
}