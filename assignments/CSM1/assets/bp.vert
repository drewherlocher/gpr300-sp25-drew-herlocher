#version 450

//Vertex attributes
layout(location = 0) in vec3 vPos;	  
layout(location = 1) in vec3 vNormal; 
layout(location = 2) in vec2 vTextureCoord; 

out vec3 vs_frag_world_position;
out vec4 vs_frag_light_position[3]; // Array for each cascade
out vec3 vs_normal;
out vec2 vs_texcoord;

uniform mat4 _Model; 
uniform mat4 _CameraViewProjection; 
uniform mat4 _LightViewProjection[3]; // Array for each cascade

void main()
{
    vec4 world_pos = _Model * vec4(vPos, 1.0);

    vs_frag_world_position = world_pos.xyz;
    // Calculate light space position for each cascade
    for (int i = 0; i < 2; i++) {
        vs_frag_light_position[i] = _LightViewProjection[i] * world_pos;
    }
    vs_normal = transpose(inverse(mat3(_Model))) * vNormal;
    vs_texcoord = vTextureCoord;

    gl_Position = _CameraViewProjection * world_pos;
}