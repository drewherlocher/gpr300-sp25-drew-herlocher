#version 450
//what is coming out of the shader
out vec4 FragColor; //The color of this fragment
in vec2 vs_texcoord;
uniform sampler2D _MainTexture; //2d texture sampler
uniform float _BlurRadius; // Blur radius control

void main()
{
    // Base offset calculated from _BlurRadius
    float offset = _BlurRadius / 300.0;
    
    // Define offsets with dynamic radius
    vec2 offsets[9] = vec2[](
        vec2(-offset, offset),
        vec2(0.0, offset),
        vec2(offset, offset),
        vec2(-offset, 0.0),
        vec2(0.0, 0.0),
        vec2(offset, 0.0),
        vec2(-offset, -offset),
        vec2(0.0, -offset),
        vec2(offset, -offset)
    );
    
    // Gaussian kernel weights (approximating a 3x3 Gaussian distribution)
    const float kernel[9] = float[](
        0.0625, 0.125, 0.0625,
        0.125,  0.25,  0.125,
        0.0625, 0.125, 0.0625
    );
    
    // Sum of all weights is 1.0
    
    vec3 average = vec3(0.0);
    for (int i = 0; i < 9; i++)
    {
        vec3 local = texture(_MainTexture, vs_texcoord.xy + offsets[i]).rgb;
        average += local * kernel[i];
    }
    
    FragColor = vec4(average, 1.0);
}