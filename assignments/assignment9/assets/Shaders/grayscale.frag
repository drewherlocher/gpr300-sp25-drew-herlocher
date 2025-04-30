#version 450

//what is coming out of the shader
out vec4 FragColor; //The color of this fragment
in vec2 vs_texcoord;

uniform sampler2D _MainTexture; //2d texture sampler

void main()
{
	vec3 albedo = texture(_MainTexture, vs_texcoord).rgb;
	float naive = (albedo.r + albedo.g + albedo.b) / 3.0;
	float average = 0.2126 * albedo.r + 0.7152 * albedo.g + 0.0722 * albedo.b; //
	FragColor = vec4(average, average, average, 1.0);
}
