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

in vec4 normal;
in vec4 fragPos;
in vec4 objOriginColor;
void main(){
    // ambient
    vec4 ambient = floatPool[1] * vec4Pool[3];
  	
    // diffuse 
    vec4 norm = normalize(normal);
    vec4 lightDir = normalize(vec4Pool[2] - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec4 diffuse = floatPool[3] * diff * vec4Pool[3];
    
    //specular
    vec4 viewDir = normalize(vec4Pool[0] - fragPos);
    vec4 reflectDir = reflect(-lightDir, norm);
    float viewValue = max(dot(viewDir, reflectDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), floatPool[10]);
    vec4 specular = floatPool[2] * spec * vec4Pool[3];

    //combination
    vec4 result = (ambient + diffuse + specular) * vec4Pool[4];
    FragColor = result;
}
