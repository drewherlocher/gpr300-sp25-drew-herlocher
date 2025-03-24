#version 450

//Vertex attributes
//telling GPU we are expecting pos and normals
layout(location = 0) in vec2 vPos;	  //Vertex position in model space
layout(location = 1) in vec2 vTextureCoord; 

out vec2 vs_texcoord;

out vec3 vs_normal;
void main()
{
	vs_texcoord = vTextureCoord;
	gl_Position = vec4(vPos.xy, 0.0, 1.0);
}
