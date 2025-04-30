#version 450 core

in vec3 WorldPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D _HeightmapTexture;
uniform float _AmbientStrength;
uniform vec3 _LightDir;
uniform vec3 _LightColor;
uniform float _SpecularStrength;
uniform float _Shininess;

// Color mapping
uniform int _UseColorMap;
uniform float _WaterLevel;
uniform vec3 _WaterColor;
uniform vec3 _LowlandColor;
uniform vec3 _HighlandColor;
uniform vec3 _MountainColor;

uniform vec3 _CameraPos;

void main()
{
    // Sample height from texture for visualization or other uses
    float height = texture(_HeightmapTexture, TexCoord).r;
    
    // Default color is based on height grayscale
    vec3 baseColor = vec3(height);
    
    // Color mapping based on height
    if (_UseColorMap == 1) {
        if (height < _WaterLevel) {
            baseColor = _WaterColor;
        }
        else if (height < 0.4) {
            float t = (height - _WaterLevel) / (0.4 - _WaterLevel);
            baseColor = mix(_WaterColor, _LowlandColor, t);
        }
        else if (height < 0.7) {
            float t = (height - 0.4) / (0.7 - 0.4);
            baseColor = mix(_LowlandColor, _HighlandColor, t);
        }
        else {
            float t = (height - 0.7) / (1.0 - 0.7);
            baseColor = mix(_HighlandColor, _MountainColor, t);
        }
    }
    
    // Ambient lighting
    vec3 ambient = _AmbientStrength * _LightColor;
    
    // Diffuse lighting
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(-_LightDir);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * _LightColor;
    
    // Specular lighting
    vec3 viewDir = normalize(_CameraPos - WorldPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), _Shininess);
    vec3 specular = _SpecularStrength * spec * _LightColor;
    
    // Final lighting result
    vec3 result = (ambient + diffuse + specular) * baseColor;
    
    // Output final color
    FragColor = vec4(result, 1.0);
}