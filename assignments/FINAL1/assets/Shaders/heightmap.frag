#version 450 core

in vec3 WorldPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FragPosLightSpace[8]; // From each cascade

out vec4 FragColor;

// Textures
uniform sampler2D _HeightmapTexture;
uniform sampler2DArray shadow_map; // Shadow map texture array

// Camera
uniform vec3 _CameraPos;

// Lighting
uniform float _AmbientStrength;
uniform vec3 _LightDir;
uniform vec3 _LightColor;
uniform vec3 _LightPos;
uniform float _SpecularStrength;
uniform float _Shininess;

// Color mapping
uniform int _UseColorMap;
uniform float _WaterLevel;
uniform vec3 _WaterColor;
uniform vec3 _LowlandColor;
uniform vec3 _HighlandColor;
uniform vec3 _MountainColor;

// Shadow mapping
uniform int enable_shadows;
uniform float bias;
uniform float minBias;
uniform float maxBias;
uniform int use_pcf;
uniform int cascade_count;
uniform float cascade_splits[8];
uniform float far_clip_plane;
uniform int visualize_cascades;

// Get cascade color for visualization
vec3 getCascadeColor(int cascadeIndex) 
{
    // Distinct colors for each cascade
    vec3 colors[8] = vec3[] 
    (
        vec3(1.0, 0.3, 0.3),  // Red for cascade 0
        vec3(0.3, 1.0, 0.3),  // Green for cascade 1
        vec3(0.3, 0.3, 1.0),  // Blue for cascade 2
        vec3(1.0, 1.0, 0.3),  // Yellow for cascade 3
        vec3(1.0, 0.3, 1.0),  // Magenta for cascade 4
        vec3(0.3, 1.0, 1.0),  // Cyan for cascade 5
        vec3(0.7, 0.7, 0.7),  // Light gray for cascade 6
        vec3(0.5, 0.5, 0.8)   // Slate blue for cascade 7
    );

    return colors[cascadeIndex];
}

// Shadow calculation function
float calculateShadow(int cascadeIndex, vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) 
{
    float shadow = 0.0;
    
    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // Check if fragment is outside light frustum
    if (projCoords.x < 0.0 || projCoords.x > 1.0 || 
        projCoords.y < 0.0 || projCoords.y > 1.0 || 
        projCoords.z < 0.0 || projCoords.z > 1.0) 
    {
        return 0.0; // Outside = no shadow
    }
    
    // Get current fragment's depth
    float currentDepth = projCoords.z;
    
    // Calculate bias based on surface angle relative to light
    float cosTheta = max(dot(normal, lightDir), 0.0);
    float adjustedBias = max(minBias, maxBias * (1.0 - cosTheta));
    
    // PCF (Percentage Closer Filtering)
    if (use_pcf == 1) 
    {
        float shadowSum = 0.0;
        vec2 texelSize = 1.0 / textureSize(shadow_map, 0).xy;
        
        for (int x = -1; x <= 1; ++x) 
        {
            for (int y = -1; y <= 1; ++y) 
            {
                float pcfDepth = texture(shadow_map, vec3(projCoords.xy + vec2(x, y) * texelSize, cascadeIndex)).r;
                shadowSum += (currentDepth - adjustedBias) > pcfDepth ? 1.0 : 0.0;
            }
        }

        shadow = shadowSum / 9.0;
    } 
    else 
    {
        // Regular shadow mapping
        float closestDepth = texture(shadow_map, vec3(projCoords.xy, cascadeIndex)).r;
        shadow = (currentDepth - adjustedBias) > closestDepth ? 1.0 : 0.0;
    }
    
    return shadow;
}

void main() {
    // Sample height from texture 
    float height = texture(_HeightmapTexture, TexCoord).r;
    
    // Default color is based on height grayscale
    vec3 baseColor = vec3(height);
    
    // Color mapping based on height
    if (_UseColorMap == 1) 
    {
        if (height < _WaterLevel) 
        {
            baseColor = _WaterColor;
        }
        else if (height < 0.4) 
        {
            float t = (height - _WaterLevel) / (0.4 - _WaterLevel);
            baseColor = mix(_WaterColor, _LowlandColor, t);
        }
        else if (height < 0.7) 
        {
            float t = (height - 0.4) / (0.7 - 0.4);
            baseColor = mix(_LowlandColor, _HighlandColor, t);
        }
        else 
        {
            float t = (height - 0.7) / (1.0 - 0.7);
            baseColor = mix(_HighlandColor, _MountainColor, t);
        }
    }
    
    // Calculate shadow if enabled
    float shadow = 0.0;
    int cascadeIndex = 0;
    
    if (enable_shadows == 1) 
    {
        // Calculate distance from camera to determine cascade
        float dist = length(_CameraPos - WorldPos);
        float normalizedDist = dist / far_clip_plane;
        
        // Find the appropriate cascade
        for (int i = 0; i < cascade_count - 1; i++) 
        {
            if (normalizedDist > cascade_splits[i]) 
            {
                cascadeIndex = i + 1;
            }
        }
        cascadeIndex = min(cascadeIndex, cascade_count - 1);
        
        // Calculate shadow
        vec3 lightDir = normalize(-_LightDir);
        shadow = calculateShadow(cascadeIndex, FragPosLightSpace[cascadeIndex], Normal, lightDir);
    }
    
    // Lighting calculations
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(-_LightDir);
    vec3 viewDir = normalize(_CameraPos - WorldPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    
    // Ambient
    vec3 ambient = _AmbientStrength * _LightColor;
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * _LightColor;
    
    // Specular
    float spec = pow(max(dot(normal, halfwayDir), 0.0), _Shininess);
    vec3 specular = _SpecularStrength * spec * _LightColor;
    
    // Combine lighting with shadow
    vec3 lighting = ambient;
    if (enable_shadows == 1) 
    {
        lighting += (diffuse + specular) * (1.0 - shadow);
    } else 
    {
        lighting += (diffuse + specular);
    }
    
    // Apply lighting to base color
    vec3 result = lighting * baseColor;
    
    // Visualize cascades if enabled
    if (enable_shadows == 1 && visualize_cascades == 1) 
    {
        vec3 cascadeColor = getCascadeColor(cascadeIndex);
        result = mix(result, cascadeColor, 0.3);
    }
    
    // Final output
    FragColor = vec4(result, 1.0);
}