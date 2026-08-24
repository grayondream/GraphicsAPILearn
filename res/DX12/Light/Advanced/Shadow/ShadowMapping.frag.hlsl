// 对应 res/GL/Light/Advanced/Shadow/ShadowMapping.fs。
// shadowMap 由 bindTexture(fbo->depthTexture2D(), 1) 绑定 → t2；GLSL 的 sampler2D 手动
// 深度比较改写为硬件比较采样：SamplerComparisonState(s9, LESS_EQUAL+ClampToBorder 白边框)
// + SampleCmp，SampleCmp 返回 stored <= ref ? 1(lit) : 0，故阴影 = 1 - lit；
// 越界边框 stored=1.0 → 恒 lit，与 GL/VK borderColor=1.0 语义一致。
// textureSize→GetDimensions；槽位照抄：viewPos=vec4Pool[0]、lightPos=vec4Pool[2]、
// enableBias=floatPool[25]、enableSimplePCF=floatPool[40]、type=floatPool[15]、debug=floatPool[18]。
#include "../../../_uniform_block.hlsli"

Texture2D gDiffuseTexture : register(t1);
Texture2D gShadowMap : register(t2);

struct PSIn {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoords : TEXCOORD2;
    float4 FragPosLightSpace : TEXCOORD3;
};

// 单点比较采样：等价 GLSL 的 (ref > closestDepth ? 1 : 0)
float ShadowPoint(float2 uv, float ref)
{
    return 1.0 - gShadowMap.SampleCmp(gShadowCompare, uv, saturate(ref));
}

float ShadowCalculation(float4 fragPosLightSpace, float3 inFragPos, float3 inNormal)
{
    // perform perspective divide
    float3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range。xy：两 API 一致的 [-1,1]→[0,1]；z：DXBuffer 的 UBO
    // 投影补丁已把 lightSpaceMatrix 的 z_ndc 映射到 D3D 裁剪空间 [0,1]（深度图存储值
    // 与 GL 视口变换逐位一致），此处再 *0.5+0.5 会双重重映射使 ref 系统性偏大→全屏误阴影
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    projCoords.z = saturate(projCoords.z);

    uint shadowWidth, shadowHeight;
    gShadowMap.GetDimensions(shadowWidth, shadowHeight);
    float2 texelSize = 1.0.xx / float2(shadowWidth, shadowHeight);

    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // bias（enableBias）
    float bias = 0.0;
    if (FPOOL(25) > 0.5) {
        float3 normal = normalize(inNormal);
        float3 lightDir = normalize(gVec4Pool[2].xyz - inFragPos);
        bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    }
    // check whether current frag pos is in shadow
    float shadow = ShadowPoint(projCoords.xy, currentDepth - bias);

    if (projCoords.z > 1.0)
        shadow = 0.0;

    if (FPOOL(40) > 0.5) {   // enableSimplePCF：9-tap
        float litSum = 0.0;
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            [unroll]
            for (int y = -1; y <= 1; ++y) {
                litSum += gShadowMap.SampleCmp(gShadowCompare,
                                               projCoords.xy + float2(x, y) * texelSize,
                                               saturate(currentDepth - bias));
            }
        }
        shadow = 1.0 - litSum / 9.0;
    }

    return shadow;
}

float4 PSMain(PSIn i) : SV_Target {
    float3 color = gDiffuseTexture.Sample(gSamplerDefault, i.TexCoords).rgb;
    float3 normal = normalize(i.Normal);
    float3 lightColor = float3(1.0, 1.0, 1.0);
    // ambient
    float3 ambient = 0.3 * lightColor;
    // diffuse
    float3 lightDir = normalize(gVec4Pool[2].xyz - i.FragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    float3 diffuse = diff * lightColor;
    // specular
    float3 viewDir = normalize(gVec4Pool[0].xyz - i.FragPos);
    float spec = pow(max(dot(normal, normalize(lightDir + viewDir)), 0.0), 64.0);   // halfwayDir
    float3 specular = spec * lightColor;
    // calculate shadow
    float shadow = ShadowCalculation(i.FragPosLightSpace, i.FragPos, i.Normal);
    float3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;

    float4 FragColor = float4(lighting, 1.0);
    if (FPOOL(15) > 1.5) {   // type
        FragColor = float4(lightColor, 1.0);
    }

    if (FPOOL(18) > 0.5) {   // debug
        if (FPOOL(15) > 0.5 && FPOOL(15) <= 1.5) {
            FragColor = float4(1.0, 0.0, 0.0, 1.0);
        } else if (FPOOL(15) > 1.5) {
            FragColor = float4(1.0, 1.0, 1.0, 1.0);
        } else {
            FragColor = float4(0.0, 1.0, 0.0, 1.0);
        }
    }

    return FragColor;
}
