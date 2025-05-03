#version 330 core
in vec4 fragColor;
in vec2 textureCoord;

out vec4 color;
uniform sampler2D textureSampler;
uniform samplerCube skyBoxSampler;
uniform vec3 cameraPos;
uniform bool enableReflection;
in vec4 normal;
in vec4 position;

void main(){
    //color = fragColor;
    if(!enableReflection){
        color = texture(textureSampler, textureCoord);
        return;
    }
    
    vec3 I = normalize(position.xyz - cameraPos.xyz);
    vec3 R = reflect(I, normalize(normal.xyz));
    color = vec4(texture(skyBoxSampler, R).rgb, 1.0);
}