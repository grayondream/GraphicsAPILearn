// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Light/Advanced/PointLightShadow/ShadowMapping.fs。
// depthMap 由 bindTexture(_shadowDepthMap, 1) 绑定 → t2（Depth32F cubemap，无 mip 链）：
// GLSL samplerCube 手动线性深度比较直译（对照 VK 版语义，不用硬件比较采样）；
// PCF 循环内的采样取 SampleLevel(lod 0)，与无 mip 链的 texture() 行为一致。
// 槽位照抄：viewPos=vec4Pool[0]、lightPos=vec4Pool[2]、far_plane=floatPool[17]、
// shadows=floatPool[19]、reverse_normals=floatPool[34]（仅 VS）、enablePCF=floatPool[40]、
// light=floatPool[45]。
#include "../../../_uniform_block.hlsli"

Texture2D gDiffuseTexture : register(t1);
TextureCube gDepthMap : register(t2);

struct PSIn {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoords : TEXCOORD2;
};

float ShadowCalculation(float3 fragPos)
{
    // get vector between fragment position and light position
    float3 fragToLight = fragPos - gVec4Pool[2].xyz;   // lightPos
    // use the fragment to light vector to sample from the depth map（存的是距离/far_plane）
    float closestDepth = gDepthMap.Sample(gSamplerCubeDepth, fragToLight).r;
    // re-transform it back to original depth value
    closestDepth *= FPOOL(17);   // far_plane
    // now get current linear depth as the length between the fragment and light position
    float currentDepth = length(fragToLight);
    // test for shadows（bias 取大值，因深度已映射回 [near_plane, far_plane] 线性域）
    float bias = 0.05;
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
    return shadow;
}

static const float3 kGridSamplingDisk[20] = {
    float3( 1,  1,  1), float3( 1, -1,  1), float3(-1, -1,  1), float3(-1,  1,  1),
    float3( 1,  1, -1), float3( 1, -1, -1), float3(-1, -1, -1), float3(-1,  1, -1),
    float3( 1,  1,  0), float3( 1, -1,  0), float3(-1, -1,  0), float3(-1,  1,  0),
    float3( 1,  0,  1), float3(-1,  0,  1), float3( 1,  0, -1), float3(-1,  0, -1),
    float3( 0,  1,  1), float3( 0, -1,  1), float3( 0, -1, -1), float3( 0,  1, -1)
};

float ShadowCalculationWithPCF(float3 fragPos)
{
    float3 fragToLight = fragPos - gVec4Pool[2].xyz;   // lightPos
    float currentDepth = length(fragToLight);

    float shadow = 0.0;
    float bias = 0.15;
    int samples = 20;
    float viewDistance = length(gVec4Pool[0].xyz - fragPos);   // viewPos
    float diskRadius = (1.0 + (viewDistance / FPOOL(17))) / 25.0;   // far_plane
    [loop]
    for (int k = 0; k < samples; ++k) {
        float closestDepth = gDepthMap.SampleLevel(gSamplerCubeDepth,
                                                   fragToLight + kGridSamplingDisk[k] * diskRadius,
                                                   0.0).r;
        closestDepth *= FPOOL(17);   // far_plane, undo mapping [0;1]
        if (currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    shadow /= float(samples);
    return shadow;
}

float4 PSMain(PSIn i) : SV_Target {
    float3 color = gDiffuseTexture.Sample(gSamplerDefault, i.TexCoords).rgb;
    float3 normal = normalize(i.Normal);
    float3 lightColor = float3(1.0, 1.0, 1.0);
    // ambient
    float3 ambient = 0.3 * lightColor;
    // diffuse
    float3 lightDir = normalize(gVec4Pool[2].xyz - i.FragPos);   // lightPos
    float diff = max(dot(lightDir, normal), 0.0);
    float3 diffuse = diff * lightColor;
    // specular
    float3 viewDir = normalize(gVec4Pool[0].xyz - i.FragPos);   // viewPos
    float spec = pow(max(dot(normal, normalize(lightDir + viewDir)), 0.0), 64.0);   // halfwayDir
    float3 specular = spec * lightColor;
    // calculate shadow
    float shadow = 0.0;
    if (FPOOL(19) > 0.5) {          // shadows
        if (FPOOL(40) > 0.5) {      // enablePCF
            shadow = ShadowCalculationWithPCF(i.FragPos);
        } else {
            shadow = ShadowCalculation(i.FragPos);
        }
    }

    float3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;
    if (FPOOL(45) > 0.5) {   // light
        return float4(lightColor, 1.0);
    }
    return float4(lighting, 1.0);
}
