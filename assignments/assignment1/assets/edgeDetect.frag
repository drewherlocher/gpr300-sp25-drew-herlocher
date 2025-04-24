#version 450
//what is coming out of the shader
out vec4 FragColor; //The color of this fragment
in vec2 vs_texcoord;
uniform sampler2D _MainTexture; //2d texture sampler
uniform float _EdgeThreshold; // Threshold for edge detection sensitivity

void main()
{
    // Sample texel size for the given texture
    vec2 texelSize = 1.0 / textureSize(_MainTexture, 0);
    
    // Sample the neighbors - Sobel operator requires 8 samples plus center
    vec3 topLeft = texture(_MainTexture, vs_texcoord + vec2(-texelSize.x, texelSize.y)).rgb;
    vec3 top = texture(_MainTexture, vs_texcoord + vec2(0, texelSize.y)).rgb;
    vec3 topRight = texture(_MainTexture, vs_texcoord + vec2(texelSize.x, texelSize.y)).rgb;
    
    vec3 left = texture(_MainTexture, vs_texcoord + vec2(-texelSize.x, 0)).rgb;
    vec3 center = texture(_MainTexture, vs_texcoord).rgb;
    vec3 right = texture(_MainTexture, vs_texcoord + vec2(texelSize.x, 0)).rgb;
    
    vec3 bottomLeft = texture(_MainTexture, vs_texcoord + vec2(-texelSize.x, -texelSize.y)).rgb;
    vec3 bottom = texture(_MainTexture, vs_texcoord + vec2(0, -texelSize.y)).rgb;
    vec3 bottomRight = texture(_MainTexture, vs_texcoord + vec2(texelSize.x, -texelSize.y)).rgb;

    vec3 sobelX = 
        -1.0 * topLeft + 1.0 * topRight +
        -2.0 * left + 2.0 * right +
        -1.0 * bottomLeft + 1.0 * bottomRight;

    vec3 sobelY = 
        -1.0 * topLeft + -2.0 * top + -1.0 * topRight +
         1.0 * bottomLeft + 2.0 * bottom + 1.0 * bottomRight;
    
    // Calculate edge intensity (gradient magnitude)
    vec3 edge = sqrt(sobelX * sobelX + sobelY * sobelY);
    
    // Apply threshold
    float intensity = (edge.r + edge.g + edge.b) / 3.0;
    if (intensity > _EdgeThreshold) {
        // Show the edge in white
        FragColor = vec4(1.0, 1.0, 1.0, 1.0);
    } else {
        // No edge detected, show black
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}