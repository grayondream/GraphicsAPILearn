#version 330 core
out vec4 FragColor;
  
uniform sampler2D textureSampler;

in vec2 TexCoord;
in vec4 opos;
in vec4 Normal;

uniform vec3 lightPos;
uniform vec4 lightColor;
uniform vec3 viewPos;
uniform bool enableBlinnPhong;

out vec4 color;
void main(){
    vec3 color = texture(textureSampler, TexCoord).rgb;
    // ambient
    vec3 ambient = 0.05 * color;
    // diffuse
    vec3 lightDir = normalize(lightPos - opos.xyz);
    vec3 normal = normalize(Normal.xyz);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * color;
    // specular
    vec3 viewDir = normalize(viewPos - opos.xyz);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = 0.0;
    if(enableBlinnPhong){
        vec3 halfwayDir = normalize(lightDir + viewDir);  
        spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    }else{
        vec3 reflectDir = reflect(-lightDir, normal);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);
    }
    vec3 specular = vec3(0.3) * spec; // assuming bright white light color
    FragColor = vec4(ambient + diffuse + specular, 1.0);
}