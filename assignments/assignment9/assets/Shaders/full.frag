#version 450

// Lighting and material structs
struct Material {
    float Ka;
    float Kd;
    float Ks;
    float Shininess;
};

// Input variables
in vec3 fragPos;
in vec3 fragNormal;
in vec2 fragTexCoord;
in vec4 fragPosLightSpace;

// Uniforms
uniform sampler2D _MainTexture;
uniform sampler2D _ShadowMap;

uniform vec3 _LightDirection;
uniform vec3 camera_pos;

uniform Material _Material;
uniform float _MinBias;
uniform float _MaxBias;
uniform float _ShadowSoftness;

// Output
out vec4 FragColor;

// Shadow calculation function
float calculateShadow(vec4 fragPosLightSpace, float bias) {
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // Check if outside light frustum
    if (projCoords.z > 1.0) {
        return 0.0;
    }
    
    // PCF (Percentage-Closer Filtering) for soft shadows
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(_ShadowMap, 0);
    
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(_ShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += projCoords.z - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    
    shadow /= 9.0;
    
    return shadow;
}

void main() {
    // Sample texture
    vec3 albedo = texture(_MainTexture, fragTexCoord).rgb;
    
    // Normalize normal and light direction
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(-_LightDirection);
    
    // Calculate view and reflection directions
    vec3 viewDir = normalize(camera_pos - fragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    
    // Ambient component
    vec3 ambient = _Material.Ka * albedo;
    
    // Diffuse component
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = _Material.Kd * diff * albedo;
    
    // Specular component
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), _Material.Shininess);
    vec3 specular = _Material.Ks * spec * vec3(1.0);
    
    // Shadow calculation
    float bias = (1.0 - dot(normal, lightDir));
    float shadow = calculateShadow(fragPosLightSpace, bias);
    
    // Combine lighting components
    vec3 lighting = (diffuse + specular);
    lighting *= 1.0 - shadow;
    lighting += ambient;
    vec3 final = lighting * albedo;
 //  final = final + vec3(texture(_ShadowMap, fragTexCoord).r);
    // Final color
    FragColor = vec4(final, 1.0);
   // FragColor = vec4(vec3(texture(_ShadowMap, fragTexCoord).r), 1.0);
}
