#version 450 core

out vec4 FragColor;

in vec2 UV;

uniform sampler2D _MainTexture;

void main()
{
    // Sample the texture
    vec4 texColor = texture(_MainTexture, UV);
    
    // For shadow maps and depth textures, we need to visualize differently
    if (texColor.r == texColor.g && texColor.g == texColor.b)
    {
        // Likely a shadow map or depth texture - use only the red channel
        // with enhanced contrast
        float depthValue = texColor.r;
        FragColor = vec4(vec3(depthValue), 1.0);
    }
    else
    {
        // For position buffer, normalize values to visible range
        // This helps because position values can be far outside [0,1]
        if (abs(texColor.r) > 10.0 || abs(texColor.g) > 10.0 || abs(texColor.b) > 10.0)
        {
            vec3 normalizedPos = normalize(texColor.rgb) * 0.5 + 0.5;
            FragColor = vec4(normalizedPos, 1.0);
        }
        // For normal buffers, remap from [-1,1] to [0,1]
        else if (texColor.r >= -1.0 && texColor.r <= 1.0 && 
                texColor.g >= -1.0 && texColor.g <= 1.0 && 
                texColor.b >= -1.0 && texColor.b <= 1.0)
        {
            vec3 normalizedNormal = texColor.rgb * 0.5 + 0.5;
            FragColor = vec4(normalizedNormal, 1.0);
        }
        // Otherwise just display as-is
        else
        {
            FragColor = texColor;
        }
    }
}