// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/SSAO/GBuffer.vs。布局以 SSAOApp::initShapes 手工交错缓冲为准：
// binding0 单槽 stride32：pos@TEXCOORD0（Float3, offset0）、normal@TEXCOORD1（Float3,
// offset12）、uv@TEXCOORD2（Float2, offset24）。backpack 模型管线复用本 shader
// （Mesh 布局语义 0/1/2 同型，多余语义忽略）。invertedNormals→floatPool[35]。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float3 aPos : TEXCOORD0;
    float3 aNormal : TEXCOORD1;
    float2 aTexCoords : TEXCOORD2;
};

struct VSOut {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float2 TexCoords : TEXCOORD1;
    float3 Normal : TEXCOORD2;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    float4x4 viewModel = mul(gView, gModel);
    float4 viewPos = mul(viewModel, float4(i.aPos, 1.0));
    o.FragPos = viewPos.xyz;
    o.TexCoords = i.aTexCoords;

    float3x3 nm = transpose(Mat3Inverse((float3x3)viewModel));
    float3 n = FPOOL(35) > 0.5 ? -i.aNormal : i.aNormal;   // invertedNormals
    o.Normal = mul(nm, n);

    o.sv = mul(gProjection, viewPos);
    return o;
}
