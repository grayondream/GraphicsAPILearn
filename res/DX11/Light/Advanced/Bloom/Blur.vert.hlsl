// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 全屏 quad 管线：binding0 交错 pos@TEXCOORD0（Float3, offset0）+ uv@TEXCOORD1
// （Float2, offset12），stride 20；TriangleStrip 4 顶点。
// 对应 res/GL/Light/Advanced/Bloom/Blur.vs（GLSL 330，无 UniformBlock，pos 直传 clip 空间）。
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
    o.TexCoords = float2(i.aTexCoords.x, 1.0 - i.aTexCoords.y);
    o.sv = float4(i.aPos, 1.0);
    return o;
}
