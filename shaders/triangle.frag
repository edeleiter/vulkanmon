#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

// Temporarily comment out texture sampling to debug
// layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    // Temporarily just use vertex colors with a texture coordinate gradient
    // This helps us verify the texture coordinates are working
    outColor = vec4(fragColor * (0.5 + 0.5 * fragTexCoord.x), 1.0);
}