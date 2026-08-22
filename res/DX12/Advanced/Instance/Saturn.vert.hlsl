// 对应 res/GL/Advanced/Instance/Saturn.vs：土星球体（Model 网格，非实例化）。
// layout 为 Model::vertexLayout()（MeshVertex 交错 b0）：pos@TEXCOORD0 + normal@TEXCOORD1 +
// uv@TEXCOORD2（Tangent/Bitangent/Bone 槽位 3..6 shader 未消费）。
#include "../../_uniform_block.hlsli"

struct VSIn {
    float3 aPos : TEXCOORD0;
    float3 aNormal : TEXCOORD1;
    float2 aTexCoords : TEXCOORD2;
};

struct VSOut {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.TexCoords = i.aTexCoords;
    o.sv = mul(gProjection, mul(gView, mul(gModel, float4(i.aPos, 1.0))));
    return o;
}
