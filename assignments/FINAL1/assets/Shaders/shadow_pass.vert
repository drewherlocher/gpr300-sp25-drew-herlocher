#version 450

//Vertex attributes
layout(location = 0) in vec3 vPos;	  

//uniforms
uniform mat4 _Model; 
uniform mat4 _LightViewProjection; 

void main()
{
	vec4 worldPos =_Model * vec4(vPos, 1.0);
	gl_Position = _LightViewProjection * worldPos;
}