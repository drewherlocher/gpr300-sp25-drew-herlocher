#version 450 core

// Input vertex data
layout (location = 0) in vec3 aPos;       // Position
layout (location = 1) in vec2 aTexCoords; // Texture coordinates

// Outputs to fragment shader
out vec2 TexCoords;
out vec3 FragPos;
out vec3 Normal;

// Uniforms
uniform mat4 _Model;
uniform mat4 _ViewProjection;

void main()
{
    // Calculate position in world space
    vec4 worldPos = _Model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    
    // Pass texture coordinates to fragment shader
    TexCoords = aTexCoords;
 
    vec3 tangent = vec3(1.0, 0.0, 0.0);
    vec3 bitangent = vec3(0.0, 0.0, 1.0);
    
    // Simple cross product to get approximated normal
    Normal = normalize(cross(bitangent, tangent));
    
    // Final vertex position in clip space
    gl_Position = _ViewProjection * worldPos;
}