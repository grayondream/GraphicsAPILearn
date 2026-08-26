// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0 编译，入口 VSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/PBR/IBL_Specular/Brdf.vs：BRDF LUT 全屏 quad。
// layout 以 IBLSpecularApp::CreateQuadBuffer 为准：binding0 交错 pos(Float3)@TEXCOORD0 +
// uv(Float2)@TEXCOORD2（stride 20），4 顶点 TriangleStrip；pos 直接作 clip 坐标。
struct VSIn {
    float4 aPos : TEXCOORD0;
    float2 aTexCoords : TEXCOORD2;
};

struct VSOut {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.TexCoords = i.aTexCoords;
    o.sv = float4(i.aPos.xyz, 1.0);
    return o;
}
