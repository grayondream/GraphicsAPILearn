// 对应 res/GL/Light/Advanced/Defer/Light.fs（13×13 点光源延迟光照 quad）。
// GBuffer 三附件 → t1/t2/t3（bindTexture unit0..2），均 Nearest+ClampToEdge → s4。
// viewPos→vec4Pool[0]、enableVolume→floatPool[46]、Radius 存 ULight.direction.w。
#include "../../../_uniform_block.hlsli"

Texture2D gPosition : register(t1);
Texture2D gNormal : register(t2);
Texture2D gAlbedoSpec : register(t3);

struct PSIn {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    // retrieve data from gbuffer
    float3 fragPos = gPosition.Sample(gSamplerNearestClamp, i.TexCoords).rgb;
    float3 normal = gNormal.Sample(gSamplerNearestClamp, i.TexCoords).rgb;
    float4 albedoSpec = gAlbedoSpec.Sample(gSamplerNearestClamp, i.TexCoords);
    float3 diffuseColor = albedoSpec.rgb;
    float specularIntensity = albedoSpec.a;

    // then calculate lighting as usual
    float3 lighting = diffuseColor * 0.1;   // hard-coded ambient component
    float3 viewDir = normalize(gVec4Pool[0].xyz - fragPos);   // viewPos
    const int NR_LIGHTS = 13 * 13;
    [loop]
    for (int n = 0; n < NR_LIGHTS; ++n) {
        float3 lightPosition = gLights[n].position.xyz;
        float3 lightColor = gLights[n].diffuse.xyz;
        float lightLinear = gLights[n].params.y;
        float lightQuadratic = gLights[n].params.z;
        float lightRadius = gLights[n].direction.w;   // Radius 存 direction.w

        float dist = 0.0;
        if (FPOOL(46) > 0.5) {   // enableVolume
            dist = length(lightPosition - fragPos);
        }

        if (dist > lightRadius) {
            continue;
        }

        // diffuse
        float3 lightDir = normalize(lightPosition - fragPos);
        float3 diffuse = max(dot(normal, lightDir), 0.0) * diffuseColor * lightColor;
        // specular
        float3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 16.0);
        float3 specular = lightColor * spec * specularIntensity;
        // attenuation
        float dist2 = length(lightPosition - fragPos);
        float attenuation = 1.0 / (1.0 + lightLinear * dist2 + lightQuadratic * dist2 * dist2);
        diffuse *= attenuation;
        specular *= attenuation;
        lighting += diffuse + specular;
    }
    return float4(lighting, 1.0);
}
