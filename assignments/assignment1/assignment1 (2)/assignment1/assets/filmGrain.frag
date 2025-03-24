#version 330 core
out vec4 FragColor;
in vec2 vs_texcoord;

uniform sampler2D _MainTexture;
uniform float _Time;

float rand(vec2 co)
{
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec3 color = texture(_MainTexture, vs_texcoord).rgb;
    
    // Simple noise effect
    float grain = rand(vs_texcoord * (_Time + 1.0)) * 0.9; // Range [-0.05, 0.05]
    
    color += vec3(grain);

    FragColor = vec4(color, 1.0);
}
