#version 430 core
in vec4 fragColor;
in vec2 textureCoord;

out vec4 color;
layout(binding = 0) uniform sampler2D textureSampler;
float near = 0.1; 
float far  = 100.0; 

float LinearizeDepth(float depth) 
{
    float z = depth * 2.0 - 1.0; // 转换为 NDC
    return (2.0 * near * far) / (far + near - z * (far - near));    
}

void main(){
    //color = fragColor;
    //color = texture(textureSampler, textureCoord);
    //color = vec4(vec3(gl_FragCoord.z), 1.0);
    float depth = LinearizeDepth(gl_FragCoord.z) / far; // 为了演示除以 far
    color = vec4(vec3(depth), 1.0);
}