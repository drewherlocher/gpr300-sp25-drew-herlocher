#version 450
out vec4 FragColor;
in vec2 vs_texcoord;

uniform sampler2D _MainTexture;  // Color buffer
uniform sampler2D _DepthTexture; // Depth buffer
uniform float _FogDensity = 0.1;
uniform float _FogStart = 0.0;
uniform float _FogEnd = 10.0;
uniform vec3 _FogColor = vec3(0.8, 0.8, 0.9);

// Function to linearize depth
float linearizeDepth(float depth, float near, float far) {
    float z = depth * 2.0 - 1.0; // Back to NDC 
    return (2.0 * near * far) / (far + near - z * (far - near));
}

void main() {
    // Get screen color
    vec4 screenColor = texture(_MainTexture, vs_texcoord);
    
    // Get depth value
    float depth = texture(_DepthTexture, vs_texcoord).r;
    
    // Convert to linear depth (using typical near and far planes)
    float linearDepth = linearizeDepth(depth, 0.1, 100.0);
    
    // Calculate fog factor
    float fogFactor;
    
    // Linear fog calculation
    fogFactor = (_FogEnd - linearDepth) / (_FogEnd - _FogStart);
    
    // Clamp fog factor
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    // Mix original color with fog color based on fog factor
    FragColor = vec4(mix(_FogColor, screenColor.rgb, fogFactor), screenColor.a);
}