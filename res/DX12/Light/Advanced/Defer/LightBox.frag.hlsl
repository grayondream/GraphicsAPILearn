// 对应 res/GL/Light/Advanced/Defer/LightBox.fs（光源盒纯色输出）。
// lightColor→vec4Pool[3]。
#include "../../../_uniform_block.hlsli"

float4 PSMain(float4 sv : SV_Position) : SV_Target {
    return float4(gVec4Pool[3].rgb, 1.0);   // lightColor
}
