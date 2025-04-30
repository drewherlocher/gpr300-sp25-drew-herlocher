#version 450 core

layout (vertices = 4) out;

// Input from vertex shader
in vec3 vPosition[];
in vec2 vTexCoord[];
in vec3 vNormal[];

// Output to tessellation evaluation shader
out vec3 tcPosition[];
out vec2 tcTexCoord[];
out vec3 tcNormal[];

// Uniforms
uniform float _TessLevel;
uniform vec3 _CameraPosition;
uniform float _MaxTessDistance;
uniform float _MinTessDistance;

void main()
{
    // Pass attributes through
    tcPosition[gl_InvocationID] = vPosition[gl_InvocationID];
    tcTexCoord[gl_InvocationID] = vTexCoord[gl_InvocationID];
    tcNormal[gl_InvocationID] = vNormal[gl_InvocationID];
    
    // Calculate tessellation levels based on distance from camera
    if (gl_InvocationID == 0)
    {
        // Calculate patch center
        vec3 center = (vPosition[0] + vPosition[1] + vPosition[2] + vPosition[3]) / 4.0;
        
        // Distance from camera to patch
        float distance = distance(_CameraPosition, center);
        
        // Calculate tessellation level based on distance
        float tessLevel;
        if (distance <= _MinTessDistance) {
            tessLevel = _TessLevel;
        } else if (distance >= _MaxTessDistance) {
            tessLevel = 1.0;
        } else {
            float factor = (distance - _MinTessDistance) / (_MaxTessDistance - _MinTessDistance);
            tessLevel = mix(_TessLevel, 1.0, factor);
        }
        
        // Round to nearest integer
        tessLevel = max(1.0, floor(tessLevel));
        
        // Set tessellation levels
        gl_TessLevelOuter[0] = tessLevel;
        gl_TessLevelOuter[1] = tessLevel;
        gl_TessLevelOuter[2] = tessLevel;
        gl_TessLevelOuter[3] = tessLevel;
        
        gl_TessLevelInner[0] = tessLevel;
        gl_TessLevelInner[1] = tessLevel;
    }
}