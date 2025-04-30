#version 450

//constants
const int MAX_CASCADES = 6;
uniform int NUM_CASCADES;

//colors for visualizing cascades
const vec3 CASCADE_COLORS[MAX_CASCADES] = vec3[]
(
    vec3(1.0, 0.0, 0.0),    //red
    vec3(0.0, 1.0, 0.0),    //green
    vec3(0.0, 0.0, 1.0),    //blue

    vec3(1.0, 0.0, 1.0),   // magenta
    vec3(0.5, 0.5, 0.5),   // gray
    vec3(0.0, 0.0, 0.0)    // black
);

//structs
struct Material
{
	vec3 ambient;		 //Ambient	(0-1)
	vec3 diffuse;		 //Diffuse  (0-1)
	vec3 specular;		 //Specular (0-1)
	float shininess;	 //specular highlight
}; 
struct Light
{
	vec3 color;
	vec3 positon;
	bool rotating;
};

//out attributes
out vec4 FragColor; 

//uniforms
uniform sampler2D shadow_maps[MAX_CASCADES];
uniform float cascade_splits[MAX_CASCADES];
uniform mat4 light_space_matrices[MAX_CASCADES];

uniform vec3 camera_pos;

uniform float bias;
uniform float max_bias;
uniform float min_bias;

uniform bool use_pcf;
uniform bool show_cascades;

uniform Material _Material;
uniform Light _Light;

//in attributes
in vec3 vs_frag_world_position;
in vec3 vs_normal;
in vec2 vs_texcoord;

int getCascadeIndex(float viewDepth) 
{
    int cascade_index = NUM_CASCADES - 1;

    for (int i = 0; i < NUM_CASCADES - 1; i++) 
    {
        if (viewDepth < cascade_splits[i]) 
        {
            cascade_index = i;
            break;
        }
    }

    cascade_index = clamp(cascade_index, 0, NUM_CASCADES - 1);

    return cascade_index;
}

float shadow_calculation(vec3 frag_pos, vec3 normal, vec3 light_dir, int cascade_index)
{
    //get light space pos
    vec4 frag_pos_light_space = light_space_matrices[cascade_index] * vec4(frag_pos, 1.0);
    
    //calc shadow
    float shadow = 0.0;
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    proj_coords = proj_coords * 0.5 + 0.5;
    
    //check if in shadow map bounds
    if (proj_coords.z > 1.0) {
        return 0.0;
    }
    
    //calc dynamic bias based on light angle
    float cosTheta = max(dot(normal, light_dir), 0.0);
    float actual_bias = max(min_bias * (1.0 - cosTheta), bias);
    actual_bias = min(actual_bias, max_bias);
    
    //apply PCF filtering if enabled
    if (use_pcf) 
    {
        vec2 texel_size = 1.0 / textureSize(shadow_maps[cascade_index], 0);

        for (int x = -1; x <= 1; ++x) 
        {
            for (int y = -1; y <= 1; ++y) 
            {
                float pcf_depth = texture(shadow_maps[cascade_index], proj_coords.xy + vec2(x, y) * texel_size).r;
                shadow += ((proj_coords.z - actual_bias) > pcf_depth) ? 1.0 : 0.0;
            }
        }

        shadow /= 9.0;
    } 
    else 
    {
        float closest_depth = texture(shadow_maps[cascade_index], proj_coords.xy).r;
        shadow = ((proj_coords.z - actual_bias) > closest_depth) ? 1.0 : 0.0;
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
	vec3 specular = pow(ndotH, _Material.shininess) * _Material.specular;

	return (diffuse + specular);
}

void main()
{
	vec3 normal = normalize(vs_normal);
	vec3 lighting = blinnphong(normal, vs_frag_world_position);
	vec3 lightDir = normalize(_Light.positon - vs_frag_world_position);

    //view space depth
	vec4 view_pos = inverse(transpose(mat4(-1.0))) * vec4(vs_frag_world_position, 1.0);
	float view_depth = abs(view_pos.z);

    int cascade_index = getCascadeIndex(view_depth);

	float shadow = shadow_calculation(vs_frag_world_position, normal, lightDir, cascade_index);

	lighting *= (1.0 - shadow);
	lighting += vec3(1.0) * _Material.ambient;
	lighting *= _Light.color;

	vec3 obj_color = normal * 0.5 + 0.5;

	//visualize cascades
	if (show_cascades) 
	{
        vec3 cascade_color = CASCADE_COLORS[cascade_index];
		FragColor = vec4(obj_color * lighting * cascade_color, 1.0);
	} 
	else 
	{
		FragColor = vec4(obj_color * lighting, 1.0);
	}
}
