#version 450 core
#extension GL_EXT_scalar_block_layout : require

out vec4 FragColor;

struct ULight { vec4 position; vec4 direction; vec4 ambient; vec4 diffuse; vec4 specular; vec4 params; };

layout(set=0, binding=0, std430) uniform UniformBlock {
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
    vec4 FragPosLightSpace;
} fs_in;

layout(set=0, binding=1) uniform sampler2D diffuseTexture;
layout(set=0, binding=2) uniform sampler2D shadowMap;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // check whether current frag pos is in shadow
    float shadow = 0.0;
    float bias = 0.0;
    if(floatPool[25] > 0.5){
        vec3 normal = normalize(fs_in.Normal);
        vec3 lightDir = normalize(vec4Pool[2].xyz - fs_in.FragPos);
        bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
        shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;
    }else{
        shadow = currentDepth > closestDepth  ? 1.0 : 0.0;
    }
    
    if(projCoords.z > 1.0)
        shadow = 0.0;

    if(floatPool[40] > 0.5){
        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
        for(int x = -1; x <= 1; ++x)
        {
            for(int y = -1; y <= 1; ++y)
            {
                float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
                shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
            }    
        }
        shadow /= 9.0;
    }
    

    return shadow;
}

void main()
{           
    vec3 color = texture(diffuseTexture, fs_in.TexCoords).rgb;
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightColor = vec3(1.0);
    // ambient
    vec3 ambient = 0.3 * lightColor;
    // diffuse
    vec3 lightDir = normalize(vec4Pool[2].xyz - fs_in.FragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * lightColor;
    // specular
    vec3 viewDir = normalize(vec4Pool[0].xyz - fs_in.FragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = 0.0;
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * lightColor;    
    // calculate shadow
    float shadow = ShadowCalculation(fs_in.FragPosLightSpace);                      
    vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;    
    
    FragColor = vec4(lighting, 1.0);
    if(floatPool[15] > 1.5){
        FragColor = vec4(lightColor, 1.0);
    }
    
    if(floatPool[18] > 0.5){
        if(floatPool[15] > 0.5 && floatPool[15] <= 1.5){
            FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        }else if(floatPool[15] > 1.5){
            FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        }else{
            FragColor = vec4(0.0, 1.0, 0.0, 1.0);
        }
    }
    
    //FragColor = vec4(vec3(color), 1.0);
}