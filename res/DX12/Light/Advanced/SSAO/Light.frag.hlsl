// 对应 res/GL/Light/Advanced/SSAO/Light.fs（单光源 + AO 调制 quad）。
// GBuffer 三附件→t1/t2/t3、ssaoColorBuffer(R32F)→t4，均 Nearest+ClampToEdge→s4。
// enableSSAO→floatPool[39]；光位为视空间（App 传 view-space lightPos），viewDir=-FragPos。
#include "../../../_uniform_block.hlsli"

Texture2D gPosition : register(t1);
Texture2D gNormal : register(t2);
Texture2D gAlbedo : register(t3);
Texture2D gSsao : register(t4);

struct PSIn {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    // retrieve data from gbuffer
    float3 fragPos = gPosition.Sample(gSamplerNearestClamp, i.TexCoords).rgb;
    float3 normal = gNormal.Sample(gSamplerNearestClamp, i.TexCoords).rgb;
    float3 diffuseColor = gAlbedo.Sample(gSamplerNearestClamp, i.TexCoords).rgb;
    float ambientOcclusion = FPOOL(39) > 0.5 ? gSsao.Sample(gSamplerNearestClamp, i.TexCoords).r : 1.0;   // enableSSAO

    float3 lightPosition = gLights[0].position.xyz;
    float3 lightColor = gLights[0].diffuse.xyz;
    float lightLinear = gLights[0].params.y;
    float lightQuadratic = gLights[0].params.z;

    // then calculate lighting as usual
    float3 ambient = 0.3 * diffuseColor * ambientOcclusion;
    float3 lighting = ambient;
    float3 viewDir = normalize(-fragPos);   // viewpos is (0.0.0)
    // diffuse
    float3 lightDir = normalize(lightPosition - fragPos);
    float3 diffuse = max(dot(normal, lightDir), 0.0) * diffuseColor * lightColor;
    // specular
    float3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 8.0);
    float3 specular = lightColor * spec;
    // attenuation
    float dist = length(lightPosition - fragPos);
    float attenuation = 1.0 / (1.0 + lightLinear * dist + lightQuadratic * dist * dist);
    diffuse *= attenuation;
    specular *= attenuation;
    lighting += diffuse + specular;

    return float4(lighting, 1.0);
}
