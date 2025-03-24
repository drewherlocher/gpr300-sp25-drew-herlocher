#version 450

//Vertex attributes
//telling GPU we are expecting pos and normals
layout(location = 0) in vec3 vPos;	  //Vertex position in model space
layout(location = 1) in vec3 vNormal; //Vertex position in model space
layout(location = 2) in vec2 vTextureCoord; //Vertex texture coordinate (UV)

//out vec3 Normal; //Output to next shader WE SWITCHED THIS FOR THIS::
out Surface
{
	//vec3 Normal; ADDED FOR DIFFUSE
	vec3 WorldPosition;	//vertex position in world space
	vec3 WorldNormal;	//vertex normal in world space
	vec2 TextCoord;
}vs_out;

//location of the model
uniform mat4 _Model; //Model->World Matrix
//the camera view projection
uniform mat4 _ViewProjection; //Combined View->Projection Matrix

void main()
{//we want to be efficient here
	//vs_out.Normal = vNormal; ADDED FOR DIFFUSE
	vs_out.WorldPosition = vec3(_Model * vec4(vPos, 1.0));
	vs_out.WorldNormal = transpose(inverse(mat3(_Model))) * vNormal;

	vs_out.TextCoord = vTextureCoord;
	//Transform vertex position to homogeneous clip space
	//gl_position = using matrices to scale vector to correct space
	//1.0 allows for movement, 0.0 holds it in place
	gl_Position = _ViewProjection * _Model * vec4(vPos, 1.0);
}
