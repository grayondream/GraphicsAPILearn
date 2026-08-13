#version 450 core
out vec4 FragColor;

struct ULight { vec4 position; vec4 direction; vec4 ambient; vec4 diffuse; vec4 specular; vec4 params; };

layout(set=0, binding=0) uniform UniformBlock {
    mat4 projection;
    mat4 view;
    mat4 model;
    mat4 normalMatrix;
    mat4 viewModel;
    mat4 extraMat4[14];
    vec4 vec4Pool[64];
    vec4 vec3Pool[64];
    float floatPool[64];
    ULight lights[6];
};

layout(set=0, binding=1) uniform sampler2D diffuseMap;
layout(set=0, binding=2) uniform sampler2D specularMap;

in vec4 normal;
in vec4 FragPos;
in vec4 objOriginColor;
in vec2 TexCoords;

vec3 CalcDirLight(ULight light, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(ULight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(ULight light, vec3 normal, vec3 fragPos, vec3 viewDir);

void main(){
    vec3 norm = normalize(normal.rgb);
    vec3 viewDir = normalize(vec4Pool[0].xyz - FragPos.rgb);

    vec3 direcLight = CalcDirLight(lights[0], norm, viewDir);
    vec3 result = direcLight;
    for(int i = 0; i < 4; i++)
        result += CalcPointLight(lights[i + 1], norm, FragPos.rgb, viewDir);
    result += CalcSpotLight(lights[5], norm, FragPos.rgb, viewDir);

    FragColor = vec4(result, 1.0);
}

vec3 CalcDirLight(ULight light, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction.xyz);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), floatPool[0]);
    vec3 ambient = light.ambient.xyz * texture(diffuseMap, TexCoords).rgb;
    vec3 diffuse = light.diffuse.xyz * diff * texture(diffuseMap, TexCoords).rgb;
    vec3 specular = light.specular.xyz * spec * texture(specularMap, TexCoords).rgb;
    return (ambient + diffuse + specular);
}

vec3 CalcPointLight(ULight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position.xyz - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), floatPool[0]);
    float distance = length(light.position.xyz - fragPos);
    float attenuation = 1.0 / (light.params.x + light.params.y * distance + light.params.z * (distance * distance));
    vec3 ambient = light.ambient.xyz * texture(diffuseMap, TexCoords).rgb;
    vec3 diffuse = light.diffuse.xyz * diff * texture(diffuseMap, TexCoords).rgb;
    vec3 specular = light.specular.xyz * spec * texture(specularMap, TexCoords).rgb;
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

vec3 CalcSpotLight(ULight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position.xyz - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), floatPool[0]);
    float distance = length(light.position.xyz - fragPos);
    float attenuation = 1.0 / (light.params.x + light.params.y * distance + light.params.z * (distance * distance));
    float theta = dot(lightDir, normalize(-light.direction.xyz));
    float epsilon = light.params.w - light.direction.w;
    float intensity = clamp((theta - light.direction.w) / epsilon, 0.0, 1.0);
    vec3 ambient = light.ambient.xyz * texture(diffuseMap, TexCoords).rgb;
    vec3 diffuse = light.diffuse.xyz * diff * texture(diffuseMap, TexCoords).rgb;
    vec3 specular = light.specular.xyz * spec * texture(specularMap, TexCoords).rgb;
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    return (ambient + diffuse + specular);
}
