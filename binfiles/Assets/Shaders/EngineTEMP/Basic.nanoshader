#type vertex
#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v_Normal;
out vec3 v_FragPos;
out vec2 v_TexCoord;

void main() {
    v_Normal = mat3(u_Model) * a_Normal;
    v_FragPos = vec3(u_Model * vec4(aPos, 1.0));
    v_TexCoord = a_TexCoord;
    gl_Position = u_Projection * u_View * u_Model * vec4(aPos, 1.0);
}

#type fragment
#version 460 core

#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

struct Light {
	int type;
	vec3 position;
	vec3 color;
	vec3 direction;
	float intensity;
	float innerCutoff;
	float outerCutoff;
	float constant;
	float linear;
	float quadratic;
};

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

#define MAX_LIGHTS 12

in vec3 v_Normal;
in vec3 v_FragPos;
in vec2 v_TexCoord;

uniform int u_numLights;
uniform Light u_lights[MAX_LIGHTS];

uniform vec3 u_CameraPos;
uniform Material u_Material;

out vec4 FragColor;

vec3 CalcDirectionalLight(Light light, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_Material.shininess);
    vec3 ambient = u_Material.ambient * light.color * light.intensity;
    vec3 diffuse = diff * u_Material.diffuse * light.color * light.intensity;
    vec3 specular = spec * u_Material.specular * light.color * light.intensity;
    return ambient + diffuse + specular;
}

vec3 CalcPointLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_Material.shininess);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
    vec3 ambient = u_Material.ambient * light.color * light.intensity;
    vec3 diffuse = diff * u_Material.diffuse * light.color * light.intensity;
    vec3 specular = spec * u_Material.specular * light.color * light.intensity;
    return (ambient + diffuse + specular) * attenuation;
}

vec3 CalcSpotLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.innerCutoff - light.outerCutoff;
    float intensity = clamp((theta - light.outerCutoff) / epsilon, 0.0, 1.0);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_Material.shininess);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
    vec3 ambient = u_Material.ambient * light.color * light.intensity;
    vec3 diffuse = diff * u_Material.diffuse * light.color * light.intensity;
    vec3 specular = spec * u_Material.specular * light.color * light.intensity;
    return (ambient + diffuse + specular) * attenuation * intensity;
}

void main() {
    //FragColor = vec4(1.0, 0.5, 0.2, 1.0);
    FragColor = vec4(normalize(v_Normal) * 0.5 + 0.5, 1.0); 
    vec3 norm = normalize(v_Normal);
    vec3 viewDir = normalize(u_CameraPos - v_FragPos);
    vec3 result = vec3(0.0);
	
	for (int i = 0; i < u_numLights; ++i) {
		if (u_lights[i].type == 0)
			result += CalcDirectionalLight(u_lights[i], norm, viewDir);
		else if (u_lights[i].type == 1)
			result += CalcPointLight(u_lights[i], norm, v_FragPos, viewDir);
		else if (u_lights[i].type == 2)
			result += CalcSpotLight(u_lights[i], norm, v_FragPos, viewDir);
	}

    FragColor = vec4(result, 1.0);
}