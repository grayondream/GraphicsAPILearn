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
    vec4 position;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;

    float constant;
    float linear;
    float quadratic;
};

uniform Light light;
uniform Material material;

in vec4 normal;
in vec4 fragPos;
in vec4 objOriginColor;
in vec2 textureCoord;

void main(){
    // ambient
    vec3 ambient = light.ambient.rgb * texture(material.diffuse, textureCoord).rgb;

    // diffuse 
    vec3 norm = normalize(normal.rgb);
    vec3 lightDir = normalize(light.position.rgb-fragPos.rgb);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse.rgb * diff * texture(material.diffuse, textureCoord).rgb;
    
    //specular
    vec3 viewDir = normalize(viewPos.rgb - fragPos.rgb);
    vec3 reflectDir = reflect(-lightDir, norm);
    float viewValue = max(dot(viewDir, reflectDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular.rgb * spec * texture(material.specular, textureCoord).rgb;

    // attenuation
    float distance    = length(light.position.rgb - fragPos.rgb);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    

    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;

    //combination
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}