#version 450

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(set = 0, binding = 2) uniform LightingData {
    vec3 directionalLightDir;
    float directionalLightIntensity;
    vec3 directionalLightColor;
    float _padding1;
    vec3 ambientColor;
    float ambientIntensity;
} lighting;

layout(set = 1, binding = 0) uniform MaterialData {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
    vec3 _padding;
} material;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 worldPos;
layout(location = 4) in vec3 cameraPos;

layout(location = 0) out vec4 outColor;

void main() {
    // Normalize the interpolated normal
    vec3 normal = normalize(fragNormal);
    
    // Sample texture for base color (optional - can be used to modulate materials)
    vec3 textureColor = texture(texSampler, fragTexCoord).rgb;
    
    // Calculate ambient lighting with material ambient property
    vec3 ambient = lighting.ambientColor * lighting.ambientIntensity * material.ambient.xyz;
    
    // Calculate directional lighting with material diffuse property
    vec3 lightDir = normalize(-lighting.directionalLightDir);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lighting.directionalLightColor * lighting.directionalLightIntensity * material.diffuse.xyz;
    
    // Calculate specular lighting with material specular property (Blinn-Phong)
    vec3 viewDir = normalize(cameraPos - worldPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    vec3 specular = spec * lighting.directionalLightColor * lighting.directionalLightIntensity * material.specular.xyz;
    
    // Combine lighting components - material properties ARE the color
    vec3 result = ambient + diffuse + specular;
    
    // Optionally modulate with texture (can be disabled for pure material testing)
    // result *= textureColor;
    
    outColor = vec4(result, 1.0);
    
    // Debug mode (commented out):
    /*
    // Normalize the interpolated normal
    vec3 normal = normalize(fragNormal);
    
    // Calculate ambient lighting with material ambient property
    vec3 ambient = lighting.ambientColor * lighting.ambientIntensity * material.ambient;
    
    // Calculate directional lighting with material diffuse property
    vec3 lightDir = normalize(-lighting.directionalLightDir);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lighting.directionalLightColor * lighting.directionalLightIntensity * material.diffuse;
    
    // Calculate specular lighting with material specular property
    vec3 viewDir = normalize(cameraPos - worldPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = spec * lighting.directionalLightColor * lighting.directionalLightIntensity * material.specular;
    
    // Combine lighting components with material properties
    vec3 result = (ambient + diffuse + specular) * fragColor;
    
    outColor = vec4(result, 1.0);
    */
}