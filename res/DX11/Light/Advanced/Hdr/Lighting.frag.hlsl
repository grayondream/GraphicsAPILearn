// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/Hdr/Lighting.fs（隧道多灯衰减光照，lights[4] 循环）。
// 纹理寄存器约定 t<unit+1>：bindTexture(_brick,0)→t1（wood.png，Load2D 默认组合 s6）。
#include "../../../_uniform_block.hlsli"

Texture2D gDiffuseTexture : register(t1);

struct PSIn {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoords : TEXCOORD2;
};

float4 PSMain(PSIn input) : SV_Target {
    float3 color = gDiffuseTexture.Sample(gSamplerDefault, input.TexCoords).rgb;
    float3 normal = normalize(input.Normal);
    // ambient
    float3 ambient = 0.0 * color;
    // lighting
    float3 lighting = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < 4; i++) {
        float3 lightPosition = gLights[i].position.xyz;
        float3 lightColor = gLights[i].diffuse.xyz;
        // diffuse
        float3 lightDir = normalize(lightPosition - input.FragPos);
        float diff = max(dot(lightDir, normal), 0.0);
        float3 diffuse = lightColor * diff * color;
        float3 result = diffuse;
        // attenuation (use quadratic as we have gamma correction)
        float distance = length(input.FragPos - lightPosition);
        result *= 1.0 / (distance * distance);
        lighting += result;
    }
    return float4(ambient + lighting, 1.0);
}
