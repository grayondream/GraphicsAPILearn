// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/Bloom/Light.fs（光源块，MRT 双输出，写 m_hdrFBO 两附件）。
// lightColor→vec4Pool[3]。
#include "../../../_uniform_block.hlsli"

struct PSIn {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoords : TEXCOORD2;
};

struct PSOut {
    float4 FragColor : SV_Target0;
    float4 BrightColor : SV_Target1;
};

PSOut PSMain(PSIn input) : SV_Target {
    PSOut o;
    o.FragColor = float4(gVec4Pool[3].rgb, 1.0);   // lightColor
    float brightness = dot(o.FragColor.rgb, float3(0.2126, 0.7152, 0.0722));
    if (brightness > 1.0)
        o.BrightColor = float4(o.FragColor.rgb, 1.0);
    else
        o.BrightColor = float4(0.0, 0.0, 0.0, 1.0);
    return o;
}
