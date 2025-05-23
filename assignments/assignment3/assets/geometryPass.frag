#version 450 core
layout(location = 0) out vec3 gPosition; //Worldspace position
layout(location = 1) out vec3 gNormal; //Worldspace normal 
layout(location = 2) out vec3 gAlbedo;

in Surface{
	vec3 WorldPosition; 
	vec3 WorldNormal;
	vec2 TextCoord;
}fs_in;

uniform sampler2D _MainTexture;
void main(){
	gPosition = fs_in.WorldPosition;
	gAlbedo = texture(_MainTexture,fs_in.TextCoord).rgb;
	gNormal = normalize(fs_in.WorldNormal);
}
