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
    ULight lights[2];
};

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs_in;

layout(set=0, binding=1) uniform sampler2D textureSampler;

vec3 BlinnPhong(vec3 normal, vec3 fragPos, vec3 lightPos, vec3 lightColor){
    // diffuse
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * lightColor;
    // specular
    vec3 viewDir = normalize(vec4Pool[0].xyz - fragPos);   // viewPos
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = 0.0;
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * lightColor;    
    // simple attenuation
    float max_distance = 1.5;
    float distance = length(lightPos - fragPos);
    float attenuation = 1.0 / (floatPool[20] > 0.5 ? distance * distance : distance);   // enableGamma
    
    diffuse *= attenuation;
    specular *= attenuation;
    
    return diffuse + specular;
}

void main(){           
    vec3 color = texture(textureSampler, fs_in.TexCoords).rgb;
    vec3 lighting = vec3(0.0);
    for(int i = 0; i < 5; ++i)
        lighting += BlinnPhong(normalize(fs_in.Normal), fs_in.FragPos, vec4Pool[13 + i].xyz, vec4Pool[29 + i].xyz);   // lightPositions[i] / lightColors[i]
    color *= lighting;
    if(floatPool[20] > 0.5){   // enableGamma
        color = pow(color, vec3(1.0/floatPool[5]));   // gammaValue
    }
        
    FragColor = vec4(color, 1.0);
    //FragColor = vec4(lightColors[3], 1.0);
}