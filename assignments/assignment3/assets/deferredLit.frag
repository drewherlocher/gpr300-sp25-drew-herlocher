#version 450 core
out vec4 FragColor; 
in vec2 UV; // From fsTriangle.vert

// Material properties
struct Material {
    float Ka; // Ambient
    float Kd; // Diffuse
    float Ks; // Specular
    float Shininess;
};

// Point Light properties
struct PointLight {
    vec3 position;
    float radius;
    vec4 color;
};

// Uniforms
uniform Material _Material;
uniform vec3 _LightDirection;
uniform vec3 _LightColor;
uniform float _LightIntensity;
uniform vec3 camera_pos;

// Shadow mapping
uniform mat4 _LightSpaceMatrix;
uniform float _MinBias;
uniform float _MaxBias;
uniform float _ShadowSoftness;

// Point Lights
#define MAX_POINT_LIGHTS 64
uniform PointLight _PointLights[MAX_POINT_LIGHTS];
uniform int _PointLightCount;

// Texture samplers
uniform layout(binding = 0) sampler2D _gPositions;
uniform layout(binding = 1) sampler2D _gNormals;
uniform layout(binding = 2) sampler2D _gAlbedo;
uniform layout(binding = 3) sampler2D _ShadowMap;

// Attenuation functions
float attenuateLinear(float distance, float radius) {
    return clamp((radius - distance) / radius, 0.0, 1.0);
}

float attenuateExponential(float distance, float radius) {
    float i = clamp(1.0 - pow(distance / radius, 4.0), 0.0, 1.0);
    return i * i;
}

float calculateShadow(vec3 worldPos, vec3 normal) {
    // Transform to light space
    vec4 posLightSpace = _LightSpaceMatrix * vec4(worldPos, 1.0);
    
    // Perspective divide
    vec3 projCoords = posLightSpace.xyz / posLightSpace.w;
    
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // Get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    
    // Check if fragment is in shadow
    float bias = max(_MinBias, _MaxBias * (1.0 - dot(normal, -_LightDirection)));
    
    // PCF (Percentage Closer Filtering)
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(_ShadowMap, 0);
    
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(_ShadowMap, projCoords.xy + vec2(x, y) * texelSize * _ShadowSoftness).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    // Keep shadow at 0.0 when outside the light's far plane region
    if(projCoords.z > 1.0)
        shadow = 0.0;
        
    return shadow;
}

vec3 calculateDirectionalLight(vec3 normal, vec3 worldPos, vec3 albedo) {
    // Calculate view direction
    vec3 viewDir = normalize(camera_pos - worldPos);
    
    // Calculate light direction
    vec3 lightDir = normalize(-_LightDirection);
    
    // Ambient component
    vec3 ambient = _Material.Ka * albedo;
    
    // Diffuse component
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = _Material.Kd * diff * albedo * _LightColor;
    
    // Specular component
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), _Material.Shininess);
    vec3 specular = _Material.Ks * spec * _LightColor;
    
    // Calculate shadow factor
    float shadow = calculateShadow(worldPos, normal);
    
    // Combine lighting components
    return ambient + (1.0 - shadow) * (diffuse + specular) * _LightIntensity;
}

vec3 calculatePointLight(PointLight light, vec3 normal, vec3 worldPos, vec3 albedo) {
    // Calculate view direction
    vec3 viewDir = normalize(camera_pos - worldPos);
    
    // Calculate light direction
    vec3 lightDir = normalize(light.position - worldPos);
    
    // Diffuse component
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = _Material.Kd * diff * albedo * light.color.rgb;
    
    // Specular component
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), _Material.Shininess);
    vec3 specular = _Material.Ks * spec * light.color.rgb;
    
    // Calculate distance and attenuation
    float distance = length(light.position - worldPos);
    float attenuation = attenuateExponential(distance, light.radius);
    
    // Combine point light components
    // Amplify the contribution to make point lights more visible
    return (diffuse + specular) * light.color.rgb * light.color.a * attenuation * 2.0;
}

void main() {
    // Sample surface properties for this screen pixel
    vec3 normal = normalize(texture(_gNormals, UV).xyz);
    vec3 worldPos = texture(_gPositions, UV).xyz;
    vec3 albedo = texture(_gAlbedo, UV).xyz;
    
    // Compute directional light
    vec3 totalLight = calculateDirectionalLight(normal, worldPos, albedo);
    
    // Compute point lights
    // Debug: Highlight point light contribution
    vec3 pointLightContribution = vec3(0.0);
    for(int i = 0; i < _PointLightCount; i++) {
        vec3 lightContrib = calculatePointLight(_PointLights[i], normal, worldPos, albedo);
        pointLightContribution += lightContrib;
        totalLight += lightContrib;
    }
    
    // If point lights are active, add a slight color tint to visualize
    if(_PointLightCount > 0) {
        // Optional: Add a debug visualization of point light contribution
        totalLight += pointLightContribution * 0.2;
    }
    
    // Final output
    FragColor = vec4(totalLight, 1.0);
}