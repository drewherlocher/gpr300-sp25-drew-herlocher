#version 450

// Vertex attributes
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

// Uniforms
uniform mat4 _Model;
uniform mat4 _ViewProjection;
uniform mat4 _LightSpaceMatrix;

// Output variables
out vec3 fragPos;
out vec3 fragNormal;
out vec2 fragTexCoord;
out vec4 fragPosLightSpace;

void main() {
    // Transform vertex position to world space
    vec4 worldPos = _Model * vec4(aPos, 1.0);
    fragPos = worldPos.xyz;
    
    // Transform normal to world space
    fragNormal = transpose(inverse(mat3(_Model))) * aNormal;
    
    // Pass texture coordinates
    fragTexCoord = aTexCoord;
    
    // Calculate position in light space
    fragPosLightSpace = _LightSpaceMatrix * worldPos;
    
    // Final vertex position
    gl_Position = _ViewProjection * worldPos;
}
