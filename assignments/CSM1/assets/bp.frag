#version 450

//structs
struct Material
{
    vec3 ambient;       //Ambient    (0-1)
    vec3 diffuse;       //Diffuse  (0-1)
    vec3 specular;      //Specular (0-1)
    float shininess;    //specular highlight
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
uniform sampler2DArray shadow_map;
uniform vec3 camera_pos;

//pcf data
uniform float bias;
uniform float maxBias;
uniform float minBias;
uniform bool use_pcf;

//cascade data
uniform int cascade_count;
uniform float cascade_splits[8];
uniform float far_clip_plane;

uniform bool visualize_cascades;

//struct
uniform Material _Material;
uniform Light _Light;

//ins
in vec3 vs_frag_world_position;
in vec4 vs_frag_light_position[8]; 
in vec3 vs_normal;
in vec2 vs_texcoord;

vec3 getCascadeColor(int cascadeIndex) 
{
    //distinct colors for each cascade
    vec3 colors[8] = vec3[]
    (
        vec3(1.0, 0.3, 0.3),  // red for cascade 0
        vec3(0.3, 1.0, 0.3),  // green for cascade 1
        vec3(0.3, 0.3, 1.0),  // blue for cascade 2
        vec3(1.0, 1.0, 0.3),  // yellow for cascade 3
        vec3(1.0, 0.3, 1.0),  // magenta for cascade 4
        vec3(0.3, 1.0, 1.0),  // cyan for cascade 5
        vec3(0.7, 0.7, 0.7),  // light gray for cascade 6
        vec3(0.5, 0.5, 0.8)   // slate blue for cascade 7
    );

    return colors[cascadeIndex];
}

float shadow_calculation(int cascadeIndex, vec4 frag_pos_lightspace, vec3 normal, vec3 lightDir)
{
    float shadow = 0.0;
    vec3 proj_coords = frag_pos_lightspace.xyz / frag_pos_lightspace.w;
    proj_coords = (proj_coords * 0.5) + 0.5;
    
    //check if fragment is outside light frustum
    if (proj_coords.x < 0.0 || proj_coords.x > 1.0 || 
       proj_coords.y < 0.0 || proj_coords.y > 1.0 || 
       proj_coords.z < 0.0 || proj_coords.z > 1.0) 
    {
        return 0.0; //outside = no shadow
    }
    
    float current_depth = proj_coords.z;
    
    if(use_pcf)
    {
        for(int x = -1; x <= 1; ++x)
        {
            for(int y = -1; y <= 1; ++y)
            {
                vec2 texelSize = 1.0 / textureSize(shadow_map, 0).xy;
                float pcf_depth = texture(shadow_map, vec3(proj_coords.xy + vec2(x, y) * texelSize, cascadeIndex)).r;
                
                shadow += ((current_depth - bias) > pcf_depth) ? 1.0 : 0.0;
            }
        }
        shadow /= 9.0;
    }
    else
    {
        float closest_depth = texture(shadow_map, vec3(proj_coords.xy, cascadeIndex)).r;
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
    vec3 specular = pow(ndotH, _Material.shininess) * _Material.specular;

    return (diffuse + specular);
}

void main()
{
    vec3 normal = normalize(vs_normal);
    vec3 lighting = blinnphong(normal, vs_frag_world_position);
    vec3 lightDir = normalize(_Light.positon - vs_frag_world_position);
    
    //calc distance from camera to determine cascade
    float dist = length(camera_pos - vs_frag_world_position);
    float normalizedDist = dist / far_clip_plane;
    
    //figure out what cascade to use based on distance
    int cascadeIndex = 0;
    for (int i = 0; i < cascade_count - 1; i++) 
    {
        if (normalizedDist > cascade_splits[i]) 
        {
            cascadeIndex = i + 1;
        }
    }
    cascadeIndex = min(cascadeIndex, cascade_count - 1);

    //calc shadow
    float shadow = shadow_calculation(cascadeIndex, vs_frag_light_position[cascadeIndex], normal, lightDir);
    
    lighting *= (1.0 - shadow);
    lighting += vec3(1.0) * _Material.ambient;
    lighting *= _Light.color;
    
    vec3 obj_color = normal * 0.5 + 0.5;
    
    //cascade colors or not
    if (visualize_cascades) 
    {
        vec3 cascade_color = getCascadeColor(cascadeIndex);
        obj_color = mix(obj_color, cascade_color, 0.5);
    }
    
    FragColor = vec4(obj_color * lighting, 1.0);
}