#version 450
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 _Model;
uniform mat4 _ViewProjection;

void main() {
    gl_Position = _ViewProjection * _Model * vec4(aPos, 1.0);
}