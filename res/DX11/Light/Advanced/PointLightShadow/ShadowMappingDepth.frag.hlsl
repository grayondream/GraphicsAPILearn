// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/PointLightShadow/ShadowMappingDepth.fs：
// 把「片元到光源的线性距离 / far_plane」写入自定义深度 SV_Depth（对应 gl_FragDepth），
// 渲染目标为 depth-only cubemap face，无颜色输出。
// 槽位照抄：lightPos=vec4Pool[2]、far_plane=floatPool[17]。
#include "../../../_uniform_block.hlsli"

struct PSIn {
    float4 sv : SV_Position;
    float4 FragPos : TEXCOORD0;
};

float PSMain(PSIn i) : SV_Depth {
    float lightDistance = length(i.FragPos.xyz - gVec4Pool[2].xyz);
    // map to [0;1] range by dividing by far_plane
    lightDistance = lightDistance / FPOOL(17);
    return lightDistance;
}
