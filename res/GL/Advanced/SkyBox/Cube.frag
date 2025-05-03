#version 330 core
in vec4 fragColor;
in vec2 textureCoord;

out vec4 color;
uniform sampler2D textureSampler;
uniform samplerCube skyBoxSampler;
uniform vec3 cameraPos;
uniform bool enableReflection;
uniform bool enableRefraction;
in vec4 normal;
in vec4 position;

void main(){
    //color = fragColor;
    if(enableReflection){
        vec3 I = normalize(position.xyz - cameraPos.xyz);
        vec3 R = reflect(I, normalize(normal.xyz));
        color = vec4(texture(skyBoxSampler, R).rgb, 1.0);
        return;
    }else if(enableRefraction){
        float ratio = 1.00 / 1.52;
        vec3 I = normalize(position.xyz - cameraPos);
        vec3 R = refract(I, normalize(normal.xyz), ratio);
        color = vec4(texture(skyBoxSampler, R).rgb, 1.0);    
        return;
    }
    
    color = texture(textureSampler, textureCoord);
}