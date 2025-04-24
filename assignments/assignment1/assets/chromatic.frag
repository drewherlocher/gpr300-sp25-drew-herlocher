#version 450
out vec4 FragColor; 
in vec2 vs_texcoord;
uniform sampler2D _MainTexture;
uniform float _ChromaticStrength;

void main()
{
    vec3 offset = vec3(_ChromaticStrength, _ChromaticStrength * 0.66, -_ChromaticStrength * 0.66);
    const vec2 direction = vec2(1.0);
    
    FragColor.r = texture(_MainTexture, vs_texcoord + (direction * vec2(offset.r))).r;
    FragColor.g = texture(_MainTexture, vs_texcoord + (direction * vec2(offset.g))).g;
    FragColor.b = texture(_MainTexture, vs_texcoord + (direction * vec2(offset.b))).b;
}