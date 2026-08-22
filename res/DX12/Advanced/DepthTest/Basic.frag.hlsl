// 对应 res/GL/Advanced/DepthTest/Basic.frag：深度可视化（LinearizeDepth(gl_FragCoord.z)），
// gl_FragCoord.z → SV_Position.z（NDC 深度）；near=0.1/far=100 为 GLSL 全局常量直译。
#include "../../_uniform_block.hlsli"

struct PSIn {
    float4 sv : SV_Position;
    float2 textureCoord : TEXCOORD0;
    float4 fragColor : TEXCOORD1;
};

static const float near = 0.1;
static const float far = 100.0;

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0;   // 转换为 NDC
    return (2.0 * near * far) / (far + near - z * (far - near));
}

float4 PSMain(PSIn i) : SV_Target {
    float depth = LinearizeDepth(i.sv.z) / far;   // 为了演示除以 far
    return float4(depth.xxx, 1.0);
}
