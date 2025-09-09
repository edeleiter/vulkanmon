#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    // Pure vertex color rendering - clean and simple
    outColor = vec4(fragColor, 1.0);
    
    // Texture functionality preserved for future use:
    // vec4 texColor = texture(texSampler, fragTexCoord);
    // outColor = texColor * vec4(fragColor, 1.0);  // Combined texture + vertex colors
}