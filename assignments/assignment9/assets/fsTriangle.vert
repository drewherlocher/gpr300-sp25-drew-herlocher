#version 450 core
out vec2 UV;

void main() {
    // This generates a triangle that covers the entire screen
    // No vertex attributes needed - we calculate them on the fly
    
    // Generate vertices for a triangle that covers the screen
    // Using a technique that generates a large triangle from the vertex ID
    UV = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(UV * 2.0 - 1.0, 0.0, 1.0);

}