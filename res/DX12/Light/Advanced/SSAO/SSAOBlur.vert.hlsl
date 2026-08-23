// 对应 res/GL/Light/Advanced/SSAO/SSAOBlur.vs（全屏 quad 管线，无 UniformBlock）。
// SSAOApp::createQuadBuffer：binding0 交错 pos@TEXCOORD0（Float3, offset0）+
// uv@TEXCOORD1（Float2, offset12），stride 20；TriangleStrip 4 顶点。
struct VSIn {
    float3 aPos : TEXCOORD0;
    float2 aTexCoords : TEXCOORD1;
};

struct VSOut {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.TexCoords = i.aTexCoords;
    o.sv = float4(i.aPos, 1.0);
    return o;
}
