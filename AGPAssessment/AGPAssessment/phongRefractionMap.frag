// Phong fragment shader phong-tex.frag matched with phong-tex.vert
#version 330

// Some drivers require the following
precision highp float;

struct lightStruct
{
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
};

struct materialStruct
{
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
	float shininess;
};

in vec3 ex_WorldNorm;
in vec3 ex_WorldView;
uniform samplerCube cubeMap;
uniform sampler2D texMap;

uniform lightStruct light;
uniform materialStruct material;
uniform sampler2D textureUnit0;

uniform float attConst;
uniform float attLinear;
uniform float attQuadratic;


in vec3 ex_N;
in vec3 ex_V;
in vec3 ex_L;
in vec2 ex_TexCoord;
in float ex_D;
layout(location = 0) out vec4 out_Color;
 
void main(void) {
    
	// Ambient intensity
	vec4 ambientI = light.ambient * material.ambient;

	// Diffuse intensity
	vec4 diffuseI = light.diffuse * material.diffuse;
	diffuseI = vec4(diffuseI.rgb * max(dot(normalize(ex_N),normalize(ex_L)),0), 1.0);

	// Specular intensity
	// Calculate R - reflection of light
	vec3 R = normalize(reflect(normalize(-ex_L),normalize(ex_N)));

	vec4 specularI = light.specular * material.specular;
	specularI = vec4(specularI.rgb * pow(max(dot(R,ex_V),0), material.shininess), 1.0);

	float attenuation=1.0f/(attConst + attLinear * ex_D + attQuadratic * ex_D*ex_D);
	vec4 tmp_Color = (diffuseI + specularI) ;//*texture(textureUnit0, ex_TexCoord); // texture removed as it makes fragments too dark with material
	//Attenuation does not affect transparency
	vec4 litColour = vec4(tmp_Color.rgb *attenuation, tmp_Color.a);
	vec4 amb=min(ambientI,vec4(1.0f));
		
	out_Color= ambientI + litColour;

	vec3 reflectTexCoord = reflect(-ex_WorldView, normalize(ex_WorldNorm));
	vec3 refractTexCoord = refract(-ex_WorldView, normalize(ex_WorldNorm), 0.66);
	out_Color =  texture(cubeMap, reflectTexCoord) * texture(cubeMap, refractTexCoord) * texture(texMap, ex_TexCoord)*litColour;

}


