#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec3 WorldPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 _Model;
uniform mat4 _ViewProjection;
uniform vec3 _CameraPos;

void main()
{
    // Calculate world position
    WorldPos = vec3(_Model * vec4(aPos, 1.0));
    
    // Pass normal in world space
    Normal = mat3(transpose(inverse(_Model))) * aNormal;
    
    // Pass texture coordinates
    TexCoord = aTexCoord;
    
    // Calculate final position
    gl_Position = _ViewProjection * vec4(WorldPos, 1.0);
}