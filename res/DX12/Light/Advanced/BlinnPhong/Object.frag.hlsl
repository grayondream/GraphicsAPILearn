// 对应 res/GL/Light/Advanced/BlinnPhong/Object.frag（Blinn-Phong 切换）。
// 纹理寄存器约定 t<unit+1>：bindTexture(_texture,0)→t1（wood.png，Load2D 默认组合 s6）。
// enableBlinnPhong→floatPool[21]。
#include "../../../_uniform_block.hlsli"

Texture2D gTextureSampler : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoords : TEXCOORD2;
};

float4 PSMain(PSIn i) : SV_Target {
    float3 color = gTextureSampler.Sample(gSamplerDefault, i.TexCoords).rgb;
    // ambient
    float3 ambient = 0.05 * color;
    // diffuse
    float3 lightDir = normalize(gLights[0].position.xyz - i.FragPos);
    float3 normal = normalize(i.Normal);
    float diff = max(dot(lightDir, normal), 0.0);
    float3 diffuse = diff * color;
    // specular
    float3 viewDir = normalize(gVec4Pool[0].xyz - i.FragPos);
    float spec = 0.0;
    if (FPOOL(21) > 0.5) {
        float3 halfwayDir = normalize(lightDir + viewDir);
        spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    } else {
        float3 reflectDir = reflect(-lightDir, normal);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);
    }
    float3 specular = float3(0.3, 0.3, 0.3) * spec;
    return float4(ambient + diffuse + specular, 1.0);
}
