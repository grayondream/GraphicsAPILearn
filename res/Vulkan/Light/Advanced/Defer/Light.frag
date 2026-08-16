#version 450 core
#extension GL_EXT_scalar_block_layout : require

out vec4 FragColor;

in vec2 TexCoords;

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
    ULight lights[256];
};

layout(set=0, binding=1) uniform sampler2D gPosition;
layout(set=0, binding=2) uniform sampler2D gNormal;
layout(set=0, binding=3) uniform sampler2D gAlbedoSpec;

struct Light {
    vec3 Position;
    vec3 Color;
    
    float Linear;
    float Quadratic;
    float Radius;
};

const int NR_LIGHTS = 13 * 13;

void main()
{             
    // retrieve data from gbuffer
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse = texture(gAlbedoSpec, TexCoords).rgb;
    float Specular = texture(gAlbedoSpec, TexCoords).a;
    
    // then calculate lighting as usual
    vec3 lighting  = Diffuse * 0.1; // hard-coded ambient component
    vec3 viewDir  = normalize(vec4Pool[0].xyz - FragPos);   // viewPos
    for(int i = 0; i < NR_LIGHTS; ++i)
    {
        Light light;
        light.Position = lights[i].position.xyz;
        light.Color = lights[i].diffuse.xyz;
        light.Linear = lights[i].params.y;
        light.Quadratic = lights[i].params.z;
        light.Radius = lights[i].direction.w;   // Radius 存 direction.w（与 quadratic/params.z 分离）

        float distance = 0;
        if(floatPool[46] > 0.5){   // enableVolume
            distance = length(light.Position - FragPos);
        }

        if(distance > light.Radius){
            continue;
        }

        {
            // diffuse
            vec3 lightDir = normalize(light.Position - FragPos);
            vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light.Color;
            // specular
            vec3 halfwayDir = normalize(lightDir + viewDir);  
            float spec = pow(max(dot(Normal, halfwayDir), 0.0), 16.0);
            vec3 specular = light.Color * spec * Specular;
            // attenuation
            float distance2 = length(light.Position - FragPos);
            float attenuation = 1.0 / (1.0 + light.Linear * distance2 + light.Quadratic * distance2 * distance2);
            diffuse *= attenuation;
            specular *= attenuation;
            lighting += diffuse + specular;        
        }
    }
    FragColor = vec4(lighting, 1.0);
}