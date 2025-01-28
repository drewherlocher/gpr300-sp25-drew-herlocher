#version 450

//what is coming out of the shader
out vec4 FragColor; //The color of this fragment
in vec2 vs_texcoord;

uniform sampler2D _MainTexture; //2d texture sampler

const vec3 offset = vec3(0.009, 0.006, -0.006);
const vec2 direction = vec2(1.0);

void main()
{
	FragColor.r = texture(_MainTexture, vs_texcoord + (direction * vec2(offset.r))).r;
	FragColor.g = texture(_MainTexture, vs_texcoord + (direction * vec2(offset.g))).g;

	FragColor.b = texture(_MainTexture, vs_texcoord + (direction * vec2(offset.b))).b;
}