#version 450
//what is coming out of the shader
out vec4 FragColor; //The color of this fragment
in vec2 vs_texcoord;
uniform sampler2D _MainTexture; //2d texture sampler
uniform float _VignetteIntensity;
uniform float _VignetteRadius;

void main()
{
    // Sample the texture
    vec4 baseColor = texture(_MainTexture, vs_texcoord);
    
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(vs_texcoord, center);
    
    float vignette = smoothstep(_VignetteRadius, 1.0, dist);
    vignette = 1.0 - (vignette * _VignetteIntensity);
    
    // Apply vignette
    FragColor = vec4(baseColor.rgb * vignette, baseColor.a);
}