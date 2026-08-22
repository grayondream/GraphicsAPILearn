// 对应 res/GL/Advanced/SkyBox/SkyBox.vert。
// layout 以 SkyboxApp 的 skyLayout 为准：仅 binding0 的 Float4@TEXCOORD0（stride 32，
// 复用 cube 交错缓冲前 16 字节）；gl_Position=clipPos.xyww 深度推到最远。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 aPos : TEXCOORD0;
};

struct VSOut {
    float4 sv : SV_Position;
    float3 TexCoords : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.TexCoords = i.aPos.xyz;
    float4 pos = mul(gProjection, mul(gView, float4(i.aPos.xyz, 1.0)));
    o.sv = float4(pos.xy, pos.w, pos.w);
    return o;
}
