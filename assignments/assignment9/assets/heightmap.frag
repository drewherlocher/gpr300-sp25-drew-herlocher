#version 450 core

// Inputs from vertex shader
in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

// Output color
out vec4 FragColor;

// Uniforms
uniform sampler2D _HeightmapTexture;

void main()
{
    // Get base color from heightmap texture
    vec4 texColor = texture(_HeightmapTexture, TexCoords);
    
    // Simple shading: darker at lower elevations, brighter at higher elevations
    // We can use the y component of FragPos as the height
    float height = (FragPos.y + 16.0) / 64.0; // Normalize height based on range used in vertex generation
    height = clamp(height, 0.0, 1.0);
    
    // Create a color gradient based on height
    vec3 lowColor = vec3(0.0, 0.2, 0.5);   // Deep blue for low areas (water)
    vec3 midColor = vec3(0.0, 0.5, 0.0);   // Green for mid-level areas
    vec3 highColor = vec3(0.8, 0.8, 0.8);  // Gray/white for high areas (mountains)
    
    vec3 terrainColor;
    if (height < 0.3) {
        // Interpolate between low and mid color
        float t = height / 0.3;
        terrainColor = mix(lowColor, midColor, t);
    } else {
        // Interpolate between mid and high color
        float t = (height - 0.3) / 0.7;
        terrainColor = mix(midColor, highColor, t);
    }
    
    // Apply simple directional lighting
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(Normal, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);
    
    // Add ambient light
    vec3 ambient = vec3(0.3);
    
    // Combine lighting and color
    vec3 result = (ambient + diffuse) * terrainColor;
    
    FragColor = vec4(result, 1.0);
}