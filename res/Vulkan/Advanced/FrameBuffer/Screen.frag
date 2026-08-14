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

in vec2 textureCoord;
in vec4 color;
out vec4 ocolor;
layout(set=0, binding=1) uniform sampler2D textureSampler;

void origin_color(){
    ocolor = texture(textureSampler, textureCoord);
}

void inversion(){
    ocolor = vec4(vec3(1.0 - texture(textureSampler, textureCoord)), 1.0);
}

void gray(){
    vec4 color = texture(textureSampler, textureCoord);
    float average = 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
    ocolor = vec4(average, average, average, 1.0);
}

const float offset = 1.0 / 300.0;  
void kernel(){
    vec2 offsets[9] = vec2[](
        vec2(-offset,  offset), // 左上
        vec2( 0.0f,    offset), // 正上
        vec2( offset,  offset), // 右上
        vec2(-offset,  0.0f),   // 左
        vec2( 0.0f,    0.0f),   // 中
        vec2( offset,  0.0f),   // 右
        vec2(-offset, -offset), // 左下
        vec2( 0.0f,   -offset), // 正下
        vec2( offset, -offset)  // 右下
    );

    float kernel[9] = float[](
        -1, -1, -1,
        -1,  9, -1,
        -1, -1, -1
    );

    vec3 sampleTex[9];
    for(int i = 0; i < 9; i++)
    {
        sampleTex[i] = vec3(texture(textureSampler, textureCoord.st + offsets[i]));
    }
    vec3 col = vec3(0.0);
    for(int i = 0; i < 9; i++)
        col += sampleTex[i] * kernel[i];

    ocolor = vec4(col, 1.0);
}

void main(){
     if(floatPool[15] > 2.5){
        kernel();
     }else if(floatPool[15] > 1.5){
        gray();
     }else if(floatPool[15] > 0.5){
        inversion();
     }else{
        origin_color();
     }
}