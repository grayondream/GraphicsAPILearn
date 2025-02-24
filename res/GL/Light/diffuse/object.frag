#version 330 core
out vec4 FragColor;
  
uniform vec4 objectColor;
uniform vec4 lightColor;
uniform vec4 lightPos;

in vec4 normal;
in vec4 fragPos;
void main(){
    // ambient
    float ambientStrength = 0.1;
    vec4 ambient = ambientStrength * lightColor;
  	
    // diffuse 
    vec4 norm = normalize(normal);
    vec4 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec4 diffuse = diff * lightColor;
            
    vec4 result = (ambient + diffuse) * objectColor;
    FragColor = result;
}