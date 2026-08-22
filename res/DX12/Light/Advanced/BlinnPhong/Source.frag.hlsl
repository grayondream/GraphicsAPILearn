// 对应 res/GL/Light/Advanced/BlinnPhong/Source.fs：color = lights[0].diffuse。
#include "../../../_uniform_block.hlsli"

float4 PSMain() : SV_Target {
    return gLights[0].diffuse;
}
