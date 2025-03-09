#version 330 core
out vec4 FragColor;
  
uniform vec4 objectColor;
uniform vec4 lightColor;
uniform vec4 lightPos;
uniform vec4 viewPos;
uniform int times;

struct Material{
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
};

struct Light{
    vec4 position;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
};

uniform Light light;
uniform Material material;
uniform sampler2D textureSampler;
in vec4 normal;
in vec4 fragPos;
in vec4 objOriginColor;
in vec2 textureCoord;

void main(){
    // ambient
    vec4 ambient = light.ambient * material.ambient;
  	
    // diffuse 
    vec4 norm = normalize(normal);
    vec4 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec4 diffuse = light.diffuse * (diff * material.diffuse);
    
    //specular
    vec4 viewDir = normalize(viewPos - fragPos);
    vec4 reflectDir = reflect(-lightDir, norm);
    float viewValue = max(dot(viewDir, reflectDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec4 specular = light.specular * (spec * material.specular);

    //combination
    vec4 result = texture(textureSampler, textureCoord);//(ambient + diffuse + specular);
    FragColor = result;
}