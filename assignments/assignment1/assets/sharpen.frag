#version 450
//what is coming out of the shader
out vec4 FragColor; //The color of this fragment
in vec2 vs_texcoord;
uniform sampler2D _MainTexture; //2d texture sampler
uniform float _SharpenStrength; // Control the strength of the sharpening effect

void main()
{
    // Sample texel size for the given texture
    vec2 texelSize = 1.0 / textureSize(_MainTexture, 0);
    
    // Get the center pixel color
    vec3 center = texture(_MainTexture, vs_texcoord).rgb;
    
    // Sample the neighbors
    vec3 top = texture(_MainTexture, vs_texcoord + vec2(0, texelSize.y)).rgb;
    vec3 right = texture(_MainTexture, vs_texcoord + vec2(texelSize.x, 0)).rgb;
    vec3 bottom = texture(_MainTexture, vs_texcoord + vec2(0, -texelSize.y)).rgb;
    vec3 left = texture(_MainTexture, vs_texcoord + vec2(-texelSize.x, 0)).rgb;
    
    vec3 sharpenResult = center * (1.0 + 4.0 * _SharpenStrength) - (top + right + bottom + left) * _SharpenStrength;
    
    // Output the final color
    FragColor = vec4(sharpenResult, 1.0);
}