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
    ULight lights[1];
};

layout(set=0, binding=1) uniform sampler2D diffuseMap;
layout(set=0, binding=2) uniform sampler2D specularMap;

in vec4 normal;
in vec4 fragPos;
in vec4 objOriginColor;
in vec2 textureCoord;

void main(){
    vec3 norm = normalize(normal.rgb);
    vec3 lightDir = normalize(lights[0].position.xyz - fragPos.rgb);
    float diff = max(dot(norm, lightDir), 0.0);

    vec3 viewDir = normalize(vec4Pool[0].xyz - fragPos.rgb);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), floatPool[0]);

    float distance = length(lights[0].position.xyz - fragPos.rgb);
    float attenuation = 1.0 / (lights[0].params.x + lights[0].params.y * distance + lights[0].params.z * (distance * distance));

    vec3 ambient = lights[0].ambient.xyz * texture(diffuseMap, textureCoord).rgb;
    vec3 diffuse = lights[0].diffuse.xyz * diff * texture(diffuseMap, textureCoord).rgb;
    vec3 specular = lights[0].specular.xyz * spec * texture(specularMap, textureCoord).rgb;

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
