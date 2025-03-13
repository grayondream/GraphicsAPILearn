#version 330 core
out vec4 FragColor;
  
uniform vec4 objectColor;
uniform vec4 lightColor;
uniform vec4 lightPos;
uniform vec4 viewPos;
uniform int times;

struct Material{
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

struct Light{
    vec4 direction;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
};

uniform Light light;
uniform Material material;

in vec4 normal;
in vec4 fragPos;
in vec4 objOriginColor;
in vec2 textureCoord;

void main(){
    // ambient
    vec4 ambient = light.ambient * vec4(texture(material.diffuse, textureCoord).rgb, 1.0);

    // diffuse 
    vec4 norm = normalize(normal);
    vec4 lightDir = normalize(-light.direction);
    float diff = max(dot(norm, lightDir), 0.0);
    vec4 diffuse = light.diffuse * diff * vec4(texture(material.diffuse, textureCoord).rgb, 1.0);
    
    //specular
    vec4 viewDir = normalize(viewPos - fragPos);
    vec4 reflectDir = reflect(-lightDir, norm);
    float viewValue = max(dot(viewDir, reflectDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec4 specular = light.specular * spec * vec4(texture(material.specular, textureCoord).rgb, 1.0);

    //combination
    vec4 result = ambient + diffuse + specular;
    FragColor = result;
}