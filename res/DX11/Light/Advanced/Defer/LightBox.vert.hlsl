// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/Defer/LightBox.vs（光源盒顶点变换，无输出 varying）。
// 物体管线布局同 GBuffer（cube 默认布局：pos@TEXCOORD0/color@TEXCOORD1/uv@TEXCOORD2/
// normal@TEXCOORD3）。对应 res/GL/Light/Advanced/Defer/LightBox.vs。
#include "../../../_uniform_block.hlsli"

struct VSIn {
    float4 aPos : TEXCOORD0;
    float4 aColor : TEXCOORD1;
    float2 aTexCoord : TEXCOORD2;
    float4 aNormal : TEXCOORD3;
};

float4 VSMain(VSIn i) : SV_Position {
    return mul(gProjection, mul(gView, mul(gModel, i.aPos)));
}
