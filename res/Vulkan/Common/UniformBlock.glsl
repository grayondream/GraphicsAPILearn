// res/Vulkan/Common/UniformBlock.glsl (镜像自 res/GL/Common)
// 布局与 CPU 侧 UniformBlock.hpp 完全一致（std140）。
// 注意：Vulkan GLSL 不允许在 uniform block 内嵌套定义 struct，ULight 须在 block 外声明。
struct ULight { vec4 position; vec4 direction; vec4 ambient; vec4 diffuse; vec4 specular; vec4 params; };

layout(set=0, binding=0) uniform UniformBlock {
    mat4  projection;
    mat4  view;
    mat4  model;
    mat4  normalMatrix;
    mat4  viewModel;
    mat4  extraMat4[14];
    vec4  vec4Pool[64];
    vec4  vec3Pool[64];
    float floatPool[64];
    ULight lights[256];
};
