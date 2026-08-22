// 对应 res/GL/Light/LightMap/Light.frag：color = vec4Pool[3]（lightColor）。
#include "../../_uniform_block.hlsli"

float4 PSMain() : SV_Target {
    return gVec4Pool[3];
}
