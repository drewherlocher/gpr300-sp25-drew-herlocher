#version 450

//what is coming out of the shader
out vec4 FragColor; //The color of this fragment
in vec2 vs_texcoord;

uniform sampler2D _MainTexture; //2d texture sampler

const float offset = 1.0 / 300.0;
const vec2 offsets[9] = vec2[](
vec2(-offset, offset),
vec2(0.0, offset),
vec2(offset, offset),

vec2(-offset, 0.0),
vec2(0.0, 0.0),
vec2(offset, 0.0),

vec2(-offset, -offset),
vec2(0.0, -offset),
vec2(offset, -offset)

);

const float strength = 16.0;
const float kernel[9] = float[](
	1.0, 1.0, 1.0,
	1.0, 1.0, 1.0,
	1.0, 1.0, 1.0


);
void main()
{
	vec3 average = vec3(0.0);

	for (int i = 0; i < 9; i++)
	{
		vec3 local = texture(_MainTexture, vs_texcoord.xy + offsets[i]).rgb;
		average += local * kernel[i] / strength;
	}
	
	FragColor = vec4(average, 1.0);
}                                      
