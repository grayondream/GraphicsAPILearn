#version 450 core
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
    ULight lights[1];
};

in vec4 fragColor;
in vec2 textureCoord;

out vec4 color;
layout(set=0, binding=1) uniform sampler2D textureSampler;
void main(){
    color = fragColor;
    if(floatPool[38] > 0.5){
        if(gl_FragCoord.x < 400)
            color = vec4(1.0, 0.0, 0.0, 1.0);
        else
            color = vec4(0.0, 1.0, 0.0, 1.0); 
    }

    if(floatPool[44] > 0.5){
        if(gl_FrontFacing){
            color = vec4(0.0, 0.0, 1.0, 1.0);
        }else{  
            color = vec4(1.0, 1.0, 0.0, 1.0);
        }

    }
    //color = texture(textureSampler, textureCoord);
}