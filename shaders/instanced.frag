#version 450

// Input from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 worldPos;
layout(location = 4) in vec3 cameraPos;
layout(location = 5) in float materialId;

// Descriptors
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
    float _pad1;
    float _pad2;
    float _pad3;
} material;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // Base color from texture and vertex color
    vec4 baseColor = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);

    // Normalize the normal vector
    vec3 normal = normalize(fragNormal);

    // Ambient lighting
    vec3 ambient = lighting.ambientColor * lighting.ambientIntensity * material.ambient.rgb;

    // Diffuse lighting
    vec3 lightDir = normalize(-lighting.directionalLightDir);
    float diffuseFactor = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = lighting.directionalLightColor * lighting.directionalLightIntensity *
                   diffuseFactor * material.diffuse.rgb;

    // Specular lighting (Blinn-Phong)
    vec3 viewDir = normalize(cameraPos - worldPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float specularFactor = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    vec3 specular = lighting.directionalLightColor * lighting.directionalLightIntensity *
                    specularFactor * material.specular.rgb;

    // Combine lighting components with base color
    vec3 lighting_result = (ambient + diffuse + specular);
    outColor = vec4(lighting_result * baseColor.rgb, baseColor.a);

    // Optional: Debug material ID with subtle color tinting
    // float matTint = materialId * 0.1;
    // outColor.rgb = mix(outColor.rgb, vec3(matTint, 0.0, 1.0 - matTint), 0.1);
}