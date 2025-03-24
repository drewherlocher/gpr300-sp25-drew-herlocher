#version 450

out vec4 frag_color;
in vec2 vs_texcoord;

uniform sampler2D texture0;

void main()
{
	float gamma = 2.2f;
	vec3 albedo = texture(texture0, vs_texcoord).rgb;
	frag_color = vec4(albedo,1.0);
	frag_color.rgb = pow(frag_color.rgb, vec3(1.0/gamma));
}