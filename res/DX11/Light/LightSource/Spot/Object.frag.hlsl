// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/LightSource/Spot/Object.frag（聚光：cutOff→params.w、outerCutOff→direction.w，
// 距离衰减 params.xyz=constant/linear/quadratic）。
// 纹理寄存器约定 t<unit+1>：bindTexture(_diffuseTex,0)→t1、bindTexture(_specularTex,1)→t2；
// 采样器取 RhiImage::Load2D 默认组合 LinearMipLinear+Repeat（s6）。
// GLSL 死接口 objOriginColor 未搬运；GLSL 块内 lights[1] 对应模板 gLights[256] 的 [0]。
#include "../../../_uniform_block.hlsli"

Texture2D gDiffuseMap : register(t1);
Texture2D gSpecularMap : register(t2);

struct PSIn {
    float4 sv : SV_Position;
    float4 normal : TEXCOORD0;
    float4 fragPos : TEXCOORD1;
    float2 textureCoord : TEXCOORD2;
};

float4 PSMain(PSIn i) : SV_Target {
    float3 norm = normalize(i.normal.rgb);
    float3 lightDir = normalize(gLights[0].position.xyz - i.fragPos.rgb);
    float diff = max(dot(norm, lightDir), 0.0);

    float3 viewDir = normalize(gVec4Pool[0].xyz - i.fragPos.rgb);
    float3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), FPOOL(0));

    float theta = dot(lightDir, normalize(-gLights[0].direction.xyz));
    float epsilon = gLights[0].params.w - gLights[0].direction.w;
    float intensity = clamp((theta - gLights[0].direction.w) / epsilon, 0.0, 1.0);

    float distance = length(gLights[0].position.xyz - i.fragPos.rgb);
    float attenuation = 1.0 / (gLights[0].params.x + gLights[0].params.y * distance + gLights[0].params.z * (distance * distance));

    float3 ambient = gLights[0].ambient.xyz * gDiffuseMap.Sample(gSamplerDefault, i.textureCoord).rgb;
    float3 diffuse = gLights[0].diffuse.xyz * diff * gDiffuseMap.Sample(gSamplerDefault, i.textureCoord).rgb;
    float3 specular = gLights[0].specular.xyz * spec * gSpecularMap.Sample(gSamplerDefault, i.textureCoord).rgb;

    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;

    float3 result = ambient + diffuse + specular;
    return float4(result, 1.0);
}
