#version 450

//what is coming out of the shader
out vec4 FragColor; //The color of this fragment

//in vec3 Normal; //Interpolated of this fragment  WE SWITHCED THIS FOR::
in Surface
{
	//vec3 Normal; ADDED FOR DIFFUSE
	vec3 WorldPosition;
	vec3 WorldNormal;
	vec2 TextCoord;
}fs_in;

//then we added this:
uniform sampler2D _MainTexture; //2d texture sampler

//ADDED FOR DIFFUSE::
uniform vec3 _LightDirection = vec3(0.0, -1.0f, 0.0);	//light pointing straight down
uniform vec3 _LightColor	 = vec3(1.0);				//a white light

//ADDED FOR SPECULAR::
uniform vec3 _EyePos;

//ADDED FOR AMBIENT::
uniform vec3 _AmbientColor = vec3(0.3,0.4,0.46);

//ADDED FOR MATERIAL CONTROLS:
struct Material
{
	float Ka;		 //Ambient	(0-1)
	float Kd;		 //Diffuse  (0-1)
	float Ks;		 //Specular (0-1)
	float Shininess; //specular highlight
}; 
uniform Material _Material;

void main()
{
	//ADDED FOR DIFFUSE
	vec3 normal = normalize(fs_in.WorldNormal);	//normalize normal bc of different screen sizes/ratios
	vec3 toLight = -_LightDirection;
	float diffuseFactor = max(dot(normal, toLight), 0.0); // clip out things that face away

	//ADDED FOR SPECULAR::
	vec3 toEye = normalize(_EyePos - fs_in.WorldPosition); //direction towards player's eyes
	vec3 h = normalize(toLight + toEye); //blinn phong uses half angle
	float specularFactor = pow(max(dot(normal, h), 0.0), _Material.Shininess);
	//apply both specular and diffuse
	//vec3 lightColor = (diffuseFactor + specularFactor) * _LightColor;
	vec3 lightColor = (_Material.Kd * diffuseFactor + _Material.Ks * specularFactor) * _LightColor;

	//ADDED FOR AMBIENT::
	lightColor += _AmbientColor * _Material.Ka;

	//amount of light diffusely reflecting off of the Surface
	//vec3 diffuseColor = _LightColor * diffuseFactor;

	vec3 objectColor = texture(_MainTexture, fs_in.TextCoord).rgb;
	FragColor = vec4(objectColor * /*diffuseColor*/ lightColor, 1.0);
}
