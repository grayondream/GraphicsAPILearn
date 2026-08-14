#version 450 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs_in;

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
    ULight lights[4];
};

layout(set=0, binding=1) uniform sampler2D diffuseTexture;

struct Light {
    vec3 Position;
    vec3 Color;
};

void main()
{           
    vec3 color = texture(diffuseTexture, fs_in.TexCoords).rgb;
    vec3 normal = normalize(fs_in.Normal);
    // ambient
    vec3 ambient = 0.1 * color;
    // lighting
    vec3 lighting = vec3(0.0);
    vec3 viewDir = normalize(vec4Pool[0].xyz - fs_in.FragPos);   // viewPos
    for(int i = 0; i < 4; i++)
    {
        Light light;
        light.Position = lights[i].position.xyz;
        light.Color = lights[i].diffuse.xyz;
        // diffuse
        vec3 lightDir = normalize(light.Position - fs_in.FragPos);
        float diff = max(dot(lightDir, normal), 0.0);
        vec3 result = light.Color * diff * color;      
        // attenuation (use quadratic as we have gamma correction)
        float distance = length(fs_in.FragPos - light.Position);
        result *= 1.0 / (distance * distance);
        lighting += result;
                
    }
    vec3 result = ambient + lighting;
    // check whether result is higher than some threshold, if so, output as bloom threshold color
    float brightness = dot(result, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        BrightColor = vec4(result, 1.0);
    else
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
    FragColor = vec4(result, 1.0);
}
