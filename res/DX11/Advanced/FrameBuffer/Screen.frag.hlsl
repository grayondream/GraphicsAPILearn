// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/FrameBuffer/Screen.frag：四效果分支（effectType=floatPool[15]）：
// 原色/反相/灰度/锐化卷积核；textureSampler 由 bindTexture(screenFbo->colorTexture2D(0), 0)→t1。
// GLSL 局部数组 offsets/kernel 改 static const（kernel 与函数同名做词法避让改 k）。
#include "../../_uniform_block.hlsli"

Texture2D gTextureSampler : register(t1);

static const float kOffset = 1.0 / 300.0;

struct PSIn {
    float4 sv : SV_Position;
    float2 textureCoord : TEXCOORD0;
    float4 color : TEXCOORD1;
};

void origin_color(inout float4 ocolor, float2 texCoord)
{
    ocolor = gTextureSampler.Sample(gSamplerDefault, texCoord);
}

void inversion(inout float4 ocolor, float2 texCoord)
{
    ocolor = float4(1.0 - gTextureSampler.Sample(gSamplerDefault, texCoord).rgb, 1.0);
}

void gray(inout float4 ocolor, float2 texCoord)
{
    float4 c = gTextureSampler.Sample(gSamplerDefault, texCoord);
    float average = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
    ocolor = float4(average, average, average, 1.0);
}

static const float2 kOffsets[9] = {
    float2(-kOffset,  kOffset), // 左上
    float2(0.0f,      kOffset), // 正上
    float2(kOffset,   kOffset), // 右上
    float2(-kOffset,  0.0f),    // 左
    float2(0.0f,      0.0f),    // 中
    float2(kOffset,   0.0f),    // 右
    float2(-kOffset, -kOffset), // 左下
    float2(0.0f,     -kOffset), // 正下
    float2(kOffset,  -kOffset)  // 右下
};

static const float kKernel[9] = {
    -1, -1, -1,
    -1,  9, -1,
    -1, -1, -1
};

void kernel(inout float4 ocolor, float2 texCoord)
{
    float3 sampleTex[9];
    [unroll]
    for (int i = 0; i < 9; i++) {
        sampleTex[i] = gTextureSampler.Sample(gSamplerDefault, texCoord + kOffsets[i]).rgb;
    }
    float3 col = float3(0.0, 0.0, 0.0);
    [unroll]
    for (int j = 0; j < 9; j++)
        col += sampleTex[j] * kKernel[j];

    ocolor = float4(col, 1.0);
}

float4 PSMain(PSIn i) : SV_Target {
    float4 ocolor = float4(0.0, 0.0, 0.0, 0.0);
    if (FPOOL(15) > 2.5) {       // effectType
        kernel(ocolor, i.textureCoord);
    } else if (FPOOL(15) > 1.5) {
        gray(ocolor, i.textureCoord);
    } else if (FPOOL(15) > 0.5) {
        inversion(ocolor, i.textureCoord);
    } else {
        origin_color(ocolor, i.textureCoord);
    }
    return ocolor;
}
