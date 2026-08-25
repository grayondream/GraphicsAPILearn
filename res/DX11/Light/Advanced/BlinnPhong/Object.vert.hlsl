// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 物体管线以 BlinnPhongApp 的 RhiGeometry::Create(plane, uv+normal, 默认布局) 为准：
// binding0 交错 pos@TEXCOORD0 + color@TEXCOORD1（stride 32）、uv@TEXCOORD2（binding1）、
// normal@TEXCOORD3（binding2）。对应 res/GL/Light/Advanced/BlinnPhong/Object.vs。
// GLSL 的 VS_OUT block 摊平为 TEXCOORD 插值器；未赋值的死输出 TexCoord/opos/Normal 未搬运。
// 注意：源码 FragPos 直取模型空间 pos.xyz（未经 model 变换），忠实保留。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 pos : TEXCOORD0;
    float4 col : TEXCOORD1;
    float2 textureCoord : TEXCOORD2;
    float4 normal : TEXCOORD3;
};

struct VSOut {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoords : TEXCOORD2;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.FragPos = i.pos.xyz;
    o.Normal = i.normal.xyz;
    o.TexCoords = i.textureCoord;
    o.sv = mul(gProjection, mul(gView, i.pos));
    return o;
}
