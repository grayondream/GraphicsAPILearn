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

uniform Material material;
in vec4 normal;
in vec4 fragPos;
in vec4 objOriginColor;

void main(){
    // ambient
    vec4 ambient =lightColor * material.ambient;
  	
    // diffuse 
    vec4 norm = normalize(normal);
    vec4 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec4 diffuse = lightColor * (diff * material.diffuse);
    
    //specular
    vec4 viewDir = normalize(viewPos - fragPos);
    vec4 reflectDir = reflect(-lightDir, norm);
    float viewValue = max(dot(viewDir, reflectDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec4 specular = lightColor * (spec * material.specular);

    //combination
    vec4 result = (ambient + diffuse + specular);
    FragColor = result;
}