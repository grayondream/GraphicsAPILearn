// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/SkyBox/Cube.frag：贴图立方体 + 反射/折射天空盒采样。
// 纹理寄存器约定 t<unit+1>：bindTexture(_texture,0)→t1、bindTexture(_skyBoxTexture,1)→t2；
// 槽位照抄：cameraPos=vec4Pool[1]、enableReflection=floatPool[36]、enableRefraction=floatPool[37]；
// 折射率 1.00/1.52（玻璃）照抄。
// 采样器：dog.jpg 用 gSamplerTex2DLodAlign(s11)、cubemap 用 gSamplerCubeLodAlign(s10)，
// 对齐 GL/VK 参考实现的隐式 LOD 约定（见 _samplers.hlsli 注释）。
#include "../../_uniform_block.hlsli"

Texture2D gTextureSampler : register(t1);
TextureCube gSkyBoxSampler : register(t2);

struct PSIn {
    float4 sv : SV_Position;
    float2 textureCoord : TEXCOORD0;
    float4 fragColor : TEXCOORD1;
    float4 normal : TEXCOORD2;
    float4 position : TEXCOORD3;
};

float4 PSMain(PSIn i) : SV_Target {
    if (FPOOL(36) > 0.5) {   // enableReflection
        float3 I = normalize(i.position.xyz - gVec4Pool[1].xyz);
        float3 R = reflect(I, normalize(i.normal.xyz));
        return float4(gSkyBoxSampler.Sample(gSamplerCubeLodAlign, R).rgb, 1.0);
    } else if (FPOOL(37) > 0.5) {   // enableRefraction
        float ratio = 1.00 / 1.52;
        float3 I = normalize(i.position.xyz - gVec4Pool[1].xyz);
        float3 R = refract(I, normalize(i.normal.xyz), ratio);
        return float4(gSkyBoxSampler.Sample(gSamplerCubeLodAlign, R).rgb, 1.0);
    }

    return gTextureSampler.Sample(gSamplerTex2DLodAlign, i.textureCoord);
}
