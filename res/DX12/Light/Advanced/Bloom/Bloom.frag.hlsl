// 对应 res/GL/Light/Advanced/Bloom/Bloom.fs（MRT：颜色 SV_Target0 + 亮部 SV_Target1，
// 写入 m_hdrFBO 的两个 RGBA16F 附件）。
// 纹理寄存器约定 t<unit+1>：bindTexture(wood,0)→t1、bindTexture(brick,0)→t1（逐 draw 重绑）；
// viewPos→vec4Pool[0]。
#include "../../../_uniform_block.hlsli"

Texture2D gDiffuseTexture : register(t1);

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
    float3 color = gDiffuseTexture.Sample(gSamplerDefault, input.TexCoords).rgb;
    float3 normal = normalize(input.Normal);
    // ambient
    float3 ambient = 0.1 * color;
    // lighting
    float3 lighting = float3(0.0, 0.0, 0.0);
    float3 viewDir = normalize(gVec4Pool[0].xyz - input.FragPos);   // viewPos
    for (int i = 0; i < 4; i++) {
        float3 lightPosition = gLights[i].position.xyz;
        float3 lightColor = gLights[i].diffuse.xyz;
        // diffuse
        float3 lightDir = normalize(lightPosition - input.FragPos);
        float diff = max(dot(lightDir, normal), 0.0);
        float3 result = lightColor * diff * color;
        // attenuation (use quadratic as we have gamma correction)
        float distance = length(input.FragPos - lightPosition);
        result *= 1.0 / (distance * distance);
        lighting += result;
    }
    float3 result = ambient + lighting;
    // check whether result is higher than some threshold, if so, output as bloom threshold color
    float brightness = dot(result, float3(0.2126, 0.7152, 0.0722));
    if (brightness > 1.0)
        o.BrightColor = float4(result, 1.0);
    else
        o.BrightColor = float4(0.0, 0.0, 0.0, 1.0);
    o.FragColor = float4(result, 1.0);
    return o;
}
