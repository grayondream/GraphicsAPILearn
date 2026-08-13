#version 450 core
out vec4 FragColor;
  
uniform vec4 objectColor;
uniform vec4 lightColor;
uniform vec4 lightPos;
uniform vec4 viewPos;
uniform float ambientStrength;
uniform float specularStrength;
uniform float diffuseStrength;
uniform int times;

in vec4 normal;
in vec4 fragPos;
in vec4 objOriginColor;
void main(){
    // ambient
    vec4 ambient = ambientStrength * lightColor;
  	
    // diffuse 
    vec4 norm = normalize(normal);
    vec4 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec4 diffuse = diffuseStrength * diff * lightColor;
    
    //specular
    vec4 viewDir = normalize(viewPos - fragPos);
    vec4 reflectDir = reflect(-lightDir, norm);
    float viewValue = max(dot(viewDir, reflectDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), times);
    vec4 specular = specularStrength * spec * lightColor;

    //combination
    vec4 result = (ambient + diffuse + specular) * objectColor;
    FragColor = result;
}