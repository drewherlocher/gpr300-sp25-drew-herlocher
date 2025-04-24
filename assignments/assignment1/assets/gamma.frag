#version 450
out vec4 frag_color;
in vec2 vs_texcoord;
uniform sampler2D _MainTexture;
uniform float _Gamma; 

void main()
{
    vec3 albedo = texture(_MainTexture, vs_texcoord).rgb;
    frag_color = vec4(albedo, 1.0);
    frag_color.rgb = pow(frag_color.rgb, vec3(1.0/_Gamma));
}