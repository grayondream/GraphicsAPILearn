#version 430 core
out vec4 FragColor;

struct ULight { vec4 position; vec4 direction; vec4 ambient; vec4 diffuse; vec4 specular; vec4 params; };

layout(binding = 0) uniform UniformBlock {
    mat4 projection;
    mat4 view;
    mat4 model;
    mat4 normalMatrix;
    mat4 viewModel;
    mat4 extraMat4[14];
    vec4 vec4Pool[64];
    vec4 vec3Pool[64];
    float floatPool[64];
    ULight lights[256];
};

layout(binding = 0) uniform sampler2D diffuseMap;
layout(binding = 1) uniform sampler2D specularMap;

in vec4 normal;
in vec4 fragPos;
in vec4 objOriginColor;
in vec2 textureCoord;

void main(){
    // ambient
    vec4 ambient = lights[0].ambient * vec4(texture(diffuseMap, textureCoord).rgb, 1.0);

    // diffuse 
    vec4 norm = normalize(normal);
    vec4 lightDir = normalize(lights[0].position - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec4 diffuse = lights[0].diffuse * diff * vec4(texture(diffuseMap, textureCoord).rgb, 1.0);
    
    //specular
    vec4 viewDir = normalize(vec4Pool[0] - fragPos);
    vec4 reflectDir = reflect(-lightDir, norm);
    float viewValue = max(dot(viewDir, reflectDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), floatPool[0]);
    vec4 specular = lights[0].specular * spec * vec4(texture(specularMap, textureCoord).rgb, 1.0);

    //combination
    vec4 result = ambient + diffuse + specular;
    FragColor = result;
}
