// 与 rhi::UniformBlock(C++ std430 内存布局) 逐字节对应的权威 cbuffer 模板。
// 偏移由 UniformBlock.cpp kSlots 的 offsetof 推导并经脚本核对（全项目正确性基石）：
//   projection@0  view@64  model@128  normalMatrix@192(mat4 容器)  viewModel@256
//   extraMat4[14]@320..1215（[0]=lightSpaceMatrix，[1+i]=shadowMatrices[i]）
//   vec4Pool[64]@1216..2239  vec3Pool[64]@2240..3263  floatPool[64]@3264..3519
//   lights[256]@3520..28095（ULight=6×float4=96B），总 28096B
// HLSL cbuffer 打包规则差异（相对 std430）的两个坑，本模板已规避：
//   1) 数组每个元素独占 16B 行 → float[64] 会占 1024B，故用 float4[16] 承载
//      （F(i) = gFloatPool[i>>2][i&3]，字节布局仍为连续 256B）；
//   2) ULight 等结构体按成员打包、整体 16B 行对齐 → 6×float4=96B stride 与 C++ 一致。
// 矩阵方向约定（全树）：CPU 经 glm::value_ptr 按列主序上传，HLSL uniform 默认
// column_major 存储 → 两侧逻辑矩阵一致；变换统一写 mul(matrix, vector)，等价
// GLSL 的 M*v。禁止 mul(vector, matrix)：数学上等于 Mᵀ·v。
#ifndef DX_UNIFORM_BLOCK_HLSLI
#define DX_UNIFORM_BLOCK_HLSLI

struct ULight {
    float4 position;   // xyz + 类型标志 w（SetLightParam "type"：0=点/聚光,1=方向光）
    float4 direction;  // xyz + w 复用（spot outerCutOff / Defer Radius）
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float4 params;     // constant(x)/linear(y)/quadratic(z)/cutOff(w)
};

cbuffer UniformBlock : register(b0) {
    float4x4 gProjection;     // @0
    float4x4 gView;           // @64
    float4x4 gModel;          // @128
    float4x4 gNormalMatrix;   // @192（mat3 写入左上 3×3）
    float4x4 gViewModel;      // @256
    float4x4 gExtraMat4[14];  // @320
    float4   gVec4Pool[64];   // @1216
    float4   gVec3Pool[64];   // @2240（C++ 为 vec4 容器承载 SSAO samples 等 vec3 数组）
    float4   gFloatPool[16];  // @3264（承载 C++ float[64]，共 256B）
    ULight   gLights[256];    // @3520
};

// floatPool 槽访问：槽 i 的字节偏移 = 3264 + 4*i
#define FPOOL(i) gFloatPool[(i) >> 2][(i) & 3]

// 静态采样器别名（槽位=f*3+w，注释见 _samplers.hlsli）随本模板一并可用
#include "_samplers.hlsli"

#endif
