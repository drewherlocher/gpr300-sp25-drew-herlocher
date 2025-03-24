#version 450

// Vertex attributes
layout(location = 0) in vec2 vPos;        // Vertex position in model space
layout(location = 1) in vec2 vNormal;     // Vertex normal in model space
layout(location = 2) in vec2 vTextureCoord; // Texture coordinates

// Uniforms
uniform mat4 model;       // Model matrix
uniform mat4 view;        // View matrix
uniform mat4 projection;  // Projection matrix

// Outputs to the fragment shader
out vec2 vs_texcoord;
out vec3 vs_normal;
out vec3 vs_frag_world_position;

void main()
{
    // Pass texture coordinates
    vs_texcoord = vTextureCoord;

    // Transform normal to world space (only if needed)
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vs_normal = normalize(normalMatrix * vec3(vNormal, 0.0)); 

    // Compute world position of the fragment
    vec4 worldPosition = model * vec4(vPos, 0.0, 1.0);
    vs_frag_world_position = worldPosition.xyz;

    // Compute final clip-space position
    gl_Position = projection * view * worldPosition;
}
    