// 对应 res/GL/Light/Advanced/ParallaxMap/ParallaxMap.fs（视差贴图：简单/陡峭/遮挡插值三模式）。
// 纹理寄存器约定 t<unit+1>：bindTexture(_brick,0)→t1、bindTexture(_brickNormal,1)→t2、
// bindTexture(_brickDisp,2)→t3；采样器取 RhiImage::Load2D 默认组合 s6。
// heightScale→floatPool[6]、enableDisp→floatPool[23]、enableSteep→floatPool[41]、
// enableOcclusion→floatPool[24]。
// DXIL 差异：陡峭法的数据依赖 while 循环为逐像素发散控制流，DXC 禁止其中使用隐式
// 梯度采样（Sample），故深度图读取改 SampleLevel(lod=0)；GLSL 的 texture() 在发散流中
// 导数本就未定义，lod0 与实际行为一致。
#include "../../../_uniform_block.hlsli"

Texture2D gDiffuseMap : register(t1);
Texture2D gNormalMap : register(t2);
Texture2D gDepthMap : register(t3);

struct PSIn {
    float4 sv : SV_Position;
    float3 FragPos : TEXCOORD0;
    float2 TexCoords : TEXCOORD1;
    float3 TangentLightPos : TEXCOORD2;
    float3 TangentViewPos : TEXCOORD3;
    float3 TangentFragPos : TEXCOORD4;
};

float2 ParallaxMapping(float2 texCoords, float3 viewDir) {
    float height = gDepthMap.Sample(gSamplerDefault, texCoords).r;
    return texCoords - viewDir.xy * (height * FPOOL(6));
}

float2 ParallaxMappingSteep(float2 texCoords, float3 viewDir) {
    // number of depth layers
    const float minLayers = 8;
    const float maxLayers = 32;
    float numLayers = lerp(maxLayers, minLayers, abs(dot(float3(0.0, 0.0, 1.0), viewDir)));
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    float2 P = viewDir.xy / viewDir.z * FPOOL(6);
    float2 deltaTexCoords = P / numLayers;

    // get initial values（发散 while 前的均匀流，可用隐式梯度采样）
    float2 currentTexCoords = texCoords;
    float currentDepthMapValue = gDepthMap.SampleLevel(gSamplerDefault, currentTexCoords, 0.0).r;

    [loop]
    while (currentLayerDepth < currentDepthMapValue) {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates（发散流内须显式 LOD）
        currentDepthMapValue = gDepthMap.SampleLevel(gSamplerDefault, currentTexCoords, 0.0).r;
        // get depth of next layer
        currentLayerDepth += layerDepth;
    }

    if (FPOOL(24) > 0.5) {
        float2 prevTexCoords = currentTexCoords + deltaTexCoords;

        // get depth after and before collision for linear interpolation
        float afterDepth = currentDepthMapValue - currentLayerDepth;
        float beforeDepth = gDepthMap.SampleLevel(gSamplerDefault, prevTexCoords, 0.0).r - currentLayerDepth + layerDepth;

        // interpolation of texture coordinates
        float weight = afterDepth / (afterDepth - beforeDepth);
        float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

        return finalTexCoords;
    }

    return currentTexCoords;
}

float4 PSMain(PSIn i) : SV_Target {
    // offset texture coordinates with Parallax Mapping
    float3 viewDir = normalize(i.TangentViewPos - i.TangentFragPos);
    float2 texCoords = i.TexCoords;
    if (FPOOL(23) > 0.5) {   // cbuffer 分支=全像素一致，域内隐式梯度采样合法
        if (FPOOL(41) > 0.5) {
            texCoords = ParallaxMappingSteep(i.TexCoords, viewDir);
        } else {
            texCoords = ParallaxMapping(i.TexCoords, viewDir);
        }

        if (texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0)
            discard;
    }

    // obtain normal from normal map
    float3 normal = gNormalMap.Sample(gSamplerDefault, texCoords).rgb;
    normal = normalize(normal * 2.0 - 1.0);

    // get diffuse color
    float3 color = gDiffuseMap.Sample(gSamplerDefault, texCoords).rgb;
    // ambient
    float3 ambient = 0.1 * color;
    // diffuse
    float3 lightDir = normalize(i.TangentLightPos - i.TangentFragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    float3 diffuse = diff * color;
    // specular
    float3 reflectDir = reflect(-lightDir, normal);
    float3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    float3 specular = float3(0.2, 0.2, 0.2) * spec;
    return float4(ambient + diffuse + specular, 1.0);
}
