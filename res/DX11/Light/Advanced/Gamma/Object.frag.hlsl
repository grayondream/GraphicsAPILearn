// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/Gamma/Object.fs（5 光源 Blinn-Phong + gamma 校正）。
// 纹理寄存器约定 t<unit+1>：bindTexture(_texture,0)→t1（wood.png，Load2D 默认组合 s6）。
// lightPositions→vec4Pool[13+i]、lightColors→vec4Pool[29+i]、
// enableGamma→floatPool[20]、gammaValue→floatPool[5]、viewPos→vec4Pool[0]。
#include "../../../_uniform_block.hlsli"

Texture2D gTextureSampler : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoords : TEXCOORD2;
};

float3 BlinnPhong(float3 normal, float3 fragPos, float3 lightPos, float3 lightColor) {
    // diffuse
    float3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    float3 diffuse = diff * lightColor;
    // specular
    float3 viewDir = normalize(gVec4Pool[0].xyz - fragPos);   // viewPos
    float spec = 0.0;
    float3 halfwayDir = normalize(lightDir + viewDir);
    spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    float3 specular = spec * lightColor;
    // simple attenuation
    float max_distance = 1.5;
    float distance = length(lightPos - fragPos);
    float attenuation = 1.0 / (FPOOL(20) > 0.5 ? distance * distance : distance);   // enableGamma

    diffuse *= attenuation;
    specular *= attenuation;

    return diffuse + specular;
}

float4 PSMain(PSIn input) : SV_Target {
    float3 color = gTextureSampler.Sample(gSamplerDefault, input.TexCoords).rgb;
    float3 lighting = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < 5; ++i)
        lighting += BlinnPhong(normalize(input.Normal), input.FragPos,
                               gVec4Pool[13 + i].xyz, gVec4Pool[29 + i].xyz);   // lightPositions[i] / lightColors[i]
    color *= lighting;
    if (FPOOL(20) > 0.5) {   // enableGamma
        color = pow(color, 1.0 / FPOOL(5));   // gammaValue
    }

    return float4(color, 1.0);
}
