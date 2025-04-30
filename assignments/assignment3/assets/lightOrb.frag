#version 450
in vec3 fragNormal;
uniform vec3 _Color;
out vec4 FragColor;
void main() {
    vec3 normal = normalize(fragNormal);
    float intensity = max(0.5, dot(normal, vec3(0.0, 0.0, 1.0)));
    FragColor = vec4(_Color * intensity, 0.7);
}