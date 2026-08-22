// 对应 res/GL/Advanced/PBR/IBL_Specular/Irradiance.fs：cubemap→irradiance 卷积 pass
// （渲染到 32×32 irradiance cubemap face）。environmentMap 由 bindTexture(m_envCubemap, 0)
// 绑定 → t1（RGB16F cubemap，本样例含 mip 链）。
// phi/theta 双层浮点条件循环内取 SampleLevel(lod 0)：规避 dxc 对动态循环内隐式梯度采样
// 的编译限制。GLSL texture() 在 32px 目标上隐式 lod≈log2(512/32)=4，与 lod0 存在模糊度
// 差异（irradiance 本身为低频卷积结果，影响有限）；像素级比对如超阈可调 lod=4.0。
#include "../../../_uniform_block.hlsli"

TextureCube gEnvironmentMap : register(t1);

static const float PI = 3.14159265359;

struct PSIn {
    float4 sv : SV_Position;
    float3 WorldPos : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    // The world vector acts as the normal of a tangent surface from the origin.
    float3 N = normalize(i.WorldPos);

    float3 irradiance = float3(0.0, 0.0, 0.0);

    // tangent space calculation from origin point
    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    float sampleDelta = 0.025;
    float nrSamples = 0.0;
    [loop]
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        [loop]
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            // spherical to cartesian (in tangent space)
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

            irradiance += gEnvironmentMap.SampleLevel(gSamplerDefault, sampleVec, 0.0).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / nrSamples);

    return float4(irradiance, 1.0);
}
