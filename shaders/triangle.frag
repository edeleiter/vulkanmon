#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(binding = 2) uniform LightingData {
    vec3 directionalLightDir;
    float directionalLightIntensity;
    vec3 directionalLightColor;
    float _padding1;
    vec3 ambientColor;
    float ambientIntensity;
} lighting;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 worldPos;

layout(location = 0) out vec4 outColor;

void main() {
    // Normalize the interpolated normal
    vec3 normal = normalize(fragNormal);
    
    // Calculate directional lighting
    vec3 lightDir = normalize(-lighting.directionalLightDir);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lighting.directionalLightColor * lighting.directionalLightIntensity;
    
    // Calculate ambient lighting
    vec3 ambient = lighting.ambientColor * lighting.ambientIntensity;
    
    // Combine lighting components
    vec3 result = (ambient + diffuse) * fragColor;
    
    outColor = vec4(result, 1.0);
    
    // Texture functionality preserved for future use:
    // vec4 texColor = texture(texSampler, fragTexCoord);
    // outColor = vec4((ambient + diffuse), 1.0) * texColor * vec4(fragColor, 1.0);
}