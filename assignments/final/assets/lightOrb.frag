#version 450
uniform vec3 _Color;
out vec4 FragColor;

void main() {
    FragColor = vec4(_Color, 1.0);
}