#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec3 WorldPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 FragPosLightSpace[8]; // For each cascade

uniform mat4 _Model;
uniform mat4 _ViewProjection;
uniform mat4 _LightViewProjection[8]; // For each cascade
uniform int cascade_count;
uniform int enable_shadows;

void main()
{
    // Calculate world position
    WorldPos = vec3(_Model * vec4(aPos, 1.0));
    
    // Pass normal in world space
    Normal = mat3(transpose(inverse(_Model))) * aNormal;
    
    // Pass texture coordinates
    TexCoord = aTexCoord;
    
    // Calculate light space positions for shadow mapping
    if (enable_shadows == 1) 
    {
        for (int i = 0; i < cascade_count; i++) 
        {
            FragPosLightSpace[i] = _LightViewProjection[i] * vec4(WorldPos, 1.0);
        }
    }
    
    // Calculate final position
    gl_Position = _ViewProjection * vec4(WorldPos, 1.0);
}