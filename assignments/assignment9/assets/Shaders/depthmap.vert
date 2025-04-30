#version 450

layout(location = 0) in vec3 aPosition;

uniform mat4 _LightSpaceMatrix;
uniform mat4 _Model;

void main()
{
    // Transform vertex to light space
    gl_Position = _LightSpaceMatrix * _Model * vec4(aPosition, 1.0);
}