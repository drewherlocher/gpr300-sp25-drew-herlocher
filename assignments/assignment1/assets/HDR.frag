#version 450
//what is coming out of the shader
out vec4 FragColor; //The color of this fragment
in vec2 vs_texcoord;
uniform sampler2D _MainTexture; //2d texture sampler
uniform float _Exposure; // Exposure parameter for HDR tone mapping
uniform int _ToneMappingType; // 0 = Reinhard, 1 = ACES

// ACES tone mapping from  https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESToneMapping(vec3 color, float exposure) {
    color *= exposure;
    // Apply the ACES filmic curve
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 ReinhardToneMapping(vec3 color, float exposure) {
    color *= exposure;
    return color / (1.0 + color);
}

void main()
{
    // Sample the texture
    vec3 hdrColor = texture(_MainTexture, vs_texcoord).rgb;
    
    // Apply tone mapping based on selected type
    vec3 mappedColor;
    if (_ToneMappingType == 1) {
        mappedColor = ACESToneMapping(hdrColor, _Exposure);
    } 
    else {
        // Default: Reinhard
        mappedColor = ReinhardToneMapping(hdrColor, _Exposure);
    }
   
    float gamma = 2.2;
    mappedColor = pow(mappedColor, vec3(1.0 / gamma));
    
    FragColor = vec4(mappedColor, 1.0);
}