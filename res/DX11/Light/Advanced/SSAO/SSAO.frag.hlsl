// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/SSAO/SSAO.fs（视空间 SSAO，64 采样核，R32F 单通道输出）。
// gPosition/gNormal→t1/t2（GBuffer 附件 Nearest+ClampToEdge→s4）、texNoise→t3
// （RGBA32F Repeat+Nearest→s3）。samples[i]→vec3Pool[i]。
#include "../../../_uniform_block.hlsli"

Texture2D gPosition : register(t1);
Texture2D gNormal : register(t2);
Texture2D gTexNoise : register(t3);

struct PSIn {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

float PSMain(PSIn i) : SV_Target {
    static const int kernelSize = 64;
    static const float radius = 0.5;
    static const float bias = 0.025;
    // tile noise texture over screen based on screen dimensions divided by noise size
    static const float2 noiseScale = float2(800.0 / 4.0, 600.0 / 4.0);

    // get input for SSAO algorithm
    float3 fragPos = gPosition.Sample(gSamplerNearestClamp, i.TexCoords).xyz;
    float3 normal = normalize(gNormal.Sample(gSamplerNearestClamp, i.TexCoords).rgb);
    float3 randomVec = normalize(gTexNoise.Sample(gSamplerNearestRepeat, i.TexCoords * noiseScale).xyz);
    // create TBN change-of-basis matrix: from tangent-space to view-space
    float3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, normal);
    // iterate over the sample kernel and calculate occlusion factor
    float occlusion = 0.0;
    [loop]
    for (int k = 0; k < kernelSize; ++k) {
        // get sample position（samples[k]→gVec3Pool[k]，从切线空间到视空间）
        float3 samplePos = mul(TBN, gVec3Pool[k].xyz);
        samplePos = fragPos + samplePos * radius;

        // project sample position (to sample texture) (to get position on screen/texture)
        float4 offset = mul(gProjection, float4(samplePos, 1.0));   // view→clip
        offset.xyz /= offset.w;   // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5;

        // get sample depth
        float sampleDepth = gPosition.Sample(gSamplerNearestClamp, offset.xy).z;

        // range check & accumulate
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / kernelSize);

    return occlusion;
}
