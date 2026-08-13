// res/GL/Common/UniformBlock.glsl
// 供所有 GL shader 在 #version 430 core 之后、layout（location=..）输入之前直接粘贴。
// 布局与 CPU 侧 UniformBlock.hpp 完全一致（std140）。
// 注意：Vulkan GLSL 不允许在 uniform block 内嵌套定义 struct，ULight 须在 block 外声明。
struct ULight { vec4 position; vec4 direction; vec4 ambient; vec4 diffuse; vec4 specular; vec4 params; };

layout(set=0, binding=0) uniform UniformBlock {
    mat4  projection;
    mat4  view;
    mat4  model;
    mat4  normalMatrix;     // CPU 容器 mat4(64B)；App 填 mat3 时展开低 3x3、其余清零
    mat4  viewModel;
    mat4  extraMat4[14];    // [0]=lightSpaceMatrix, [1..6]=shadowMatrices[i]
    vec4  vec4Pool[64];     // viewPos/cameraPos/lightPos/lightColor/objectColor/...
    vec4  vec3Pool[64];     // 用 vec4 承载（w 恒 0），SSAO samples 落此
    float floatPool[64];    // 标量（含开关 float）
    ULight lights[256];
};
