#version 450

//structs
struct Material
{
	vec3 ambient;		 //Ambient	(0-1)
	vec3 diffuse;		 //Diffuse  (0-1)
	vec3 specular;		 //Specular (0-1)
	float shininess; //specular highlight
}; 
struct Light
{
	vec3 color;
	vec3 positon;
	bool rotating;
};

//outs
out vec4 FragColor; 

//uniforms
uniform sampler2D shadow_map;
uniform vec3 camera_pos;

uniform float bias;
uniform float maxBias;
uniform float minBias;
uniform bool use_pcf;

uniform Material _Material;
uniform Light _Light;

//ins
in vec3 vs_frag_world_position;
in vec4 vs_frag_light_position;
in vec3 vs_normal;
in vec2 vs_texcoord;

float shadow_calculation(vec4 frag_pos_lightspace, vec3 normal, vec3 lightDir)
{
	float shadow = 0.0;

	//perspective divide for normalized device coords
	//transform to range [0, 1] for sampling
	vec3 proj_coords = frag_pos_lightspace.xyz / frag_pos_lightspace.w;
	proj_coords = (proj_coords * 0.5) + 0.5;

	if(use_pcf)
	{
		for(int x = -1; x <= 1; ++x)
		{
			for(int y = -1; y <= 1; ++y)
			{
				vec2 texelSize = 1.0 / textureSize(shadow_map, 0);
				float closest_depth = texture(shadow_map, proj_coords.xy + vec2(x, y) * texelSize).r;	//aka light_depth

				float current_depth = proj_coords.z;							//aka camera_depth
				shadow += ((current_depth - bias) > closest_depth) ? 1.0 : 0.0;

				shadow /= 9.0f;
			}
		}
	}
	else
	{
		float closest_depth = texture(shadow_map, proj_coords.xy).r;	//aka light_depth
		float current_depth = proj_coords.z;							//aka camera_depth
		shadow += ((current_depth - bias) > closest_depth) ? 1.0 : 0.0;
	}
	return shadow;
}

vec3 blinnphong(vec3 normal, vec3 frag_pos)
{
	//normalize inputs
	vec3 view_direction = normalize(camera_pos - frag_pos);
	vec3 light_direction = normalize(_Light.positon - frag_pos);
	vec3 halfway_dir = normalize(light_direction + view_direction);

	//dot products
	float ndotL = max(dot(normal, light_direction), 0.0);
	float ndotH = max(dot(normal, halfway_dir), 0.0);

	//light components
	vec3 diffuse = ndotL * _Material.diffuse; 
	vec3 specular = pow(ndotH, _Material.shininess * 128.0) * _Material.specular;

	return (diffuse + specular);
}

void main()
{
	vec3 normal = normalize(vs_normal);
	vec3 lighting = blinnphong(normal, vs_frag_world_position);
	vec3 lightDir = normalize(_Light.positon - vs_frag_world_position);

	float shadow = shadow_calculation(vs_frag_light_position, normal, lightDir);

	lighting *= (1.0 - shadow);
	lighting += vec3(1.0) * _Material.ambient;
	lighting *= _Light.color;

	vec3 obj_color = normal * 0.5 + 0.5;

	FragColor = vec4(obj_color * lighting, 1.0);
}
