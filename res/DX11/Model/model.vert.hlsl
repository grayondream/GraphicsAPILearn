// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0 编译，入口 VSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Model/model.vert：assimp 加载模型（LoadModelApp）。
// layout 为 Model::vertexLayout()（MeshVertex 交错 b0）：pos@TEXCOORD0 + normal@TEXCOORD1 +
// uv@TEXCOORD2（Tangent/Bitangent/Bone 槽位 3..6 shader 未消费，IA 多余语义不读）。
#include "../_uniform_block.hlsli"

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
