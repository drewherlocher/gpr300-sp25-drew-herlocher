#version 450
//what is coming out of the shader
out vec4 FragColor; //The color of this fragment
in vec2 vs_texcoord;
uniform sampler2D _MainTexture; //2d texture sampler
uniform float _DistortionStrength; // Controls barrel/pincushion distortion
uniform int _DistortionType; // 0 = Barrel, 1 = Pincushion

void main()
{
    vec2 uv = vs_texcoord * 2.0 - 1.0;

    float dist = length(uv);

    float distortionFactor;
    
    if (_DistortionType == 0) {

        distortionFactor = 1.0 - _DistortionStrength * dist * dist;
    } else {
        distortionFactor = 1.0 + _DistortionStrength * dist * dist;
    }
    
    vec2 distortedUV = uv * distortionFactor;
    
    distortedUV = (distortedUV + 1.0) * 0.5;
    

    if (distortedUV.x < 0.0 || distortedUV.x > 1.0 || 
        distortedUV.y < 0.0 || distortedUV.y > 1.0) {

        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        FragColor = texture(_MainTexture, distortedUV);
    }
}