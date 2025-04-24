#version 330 core
out vec4 FragColor;
in vec2 vs_texcoord;
uniform sampler2D _MainTexture;
uniform float _Time;
uniform float _GrainIntensity;

float rand(vec2 co)
{
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec3 color = texture(_MainTexture, vs_texcoord).rgb;
    
    float grain = rand(vs_texcoord * (_Time + 1.0)) * _GrainIntensity;
    
    color += vec3(grain);
    FragColor = vec4(color, 1.0);
}