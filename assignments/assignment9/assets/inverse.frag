#version 450

//what is coming out of the shader
out vec4 FragColor; //The color of this fragment
in vec2 vs_texcoord;

uniform sampler2D _MainTexture; //2d texture sampler

void main()
{
	vec3 albedo = 1.0 - texture(_MainTexture, vs_texcoord).rgb;
	FragColor = vec4(albedo, 1.0);
}
