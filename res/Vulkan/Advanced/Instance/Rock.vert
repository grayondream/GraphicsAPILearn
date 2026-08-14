#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in mat4 aInstanceMatrix;

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

out vec2 TexCoords;

void main() {
    float radiuse = 20.0; // 半径
    TexCoords = aTexCoords;

    // 计算逆时针旋转角度
    float angle = -floatPool[10] / 10; // 逆时针旋转

    // 计算新的位置偏移
    float xOffset = radiuse * cos(angle);
    float zOffset = radiuse * sin(angle);

    // 创建旋转矩阵
    mat4 rotationMatrix = mat4(
        vec4(cos(angle), 0.0, sin(angle), 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(-sin(angle), 0.0, cos(angle), 0.0),
        vec4(0.0, 0.0, 0.0, 1.0)
    );

    // 更新实例矩阵，添加平移到中心点
    mat4 translationMatrix = mat4(1.0);
    translationMatrix[3] = vec4(vec4Pool[45].xyz, 1.0);

    // 计算新的实例矩阵
    mat4 newInstanceMatrix = translationMatrix * rotationMatrix * aInstanceMatrix;

    // 计算最终位置
    gl_Position = projection * view * newInstanceMatrix * vec4(aPos, 1.0);
}