#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;

// Output to tessellation control shader
out vec3 vPosition;
out vec2 vTexCoord;
out vec3 vNormal;

// We don't transform in vertex shader when using tessellation
// This is passed directly to tessellation control shader

void main() {
    vPosition = aPos;
    vTexCoord = aTexCoord;
    vNormal = aNormal;
}