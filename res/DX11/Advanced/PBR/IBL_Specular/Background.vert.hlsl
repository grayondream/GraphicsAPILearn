// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0 编译，入口 VSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/PBR/IBL_IC/Background.vs：天空盒（深度推到最远 clipPos.xyww）。
// mat4(mat3(view)) 数学等价 rotView*vec4(p,1) = vec4((mat3)view*p, 1)，直接以 float3x3 变换。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 col : TEXCOORD1;
    float2 aTexCoords : TEXCOORD2;
    float4 normal : TEXCOORD3;
};

struct VSOut {
    float4 sv : SV_Position;
    float3 WorldPos : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.WorldPos = i.pos.xyz;

    float3 rv = mul((float3x3)gView, i.pos.xyz);
    float4 clipPos = mul(gProjection, float4(rv, 1.0));

    // gl_Position = clipPos.xyww：z=w 使 NDC 深度恒为 1（最远）
    o.sv = float4(clipPos.xy, clipPos.w, clipPos.w);
    return o;
}
