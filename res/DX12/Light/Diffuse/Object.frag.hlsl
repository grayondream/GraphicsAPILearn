// 对应 res/GL/Light/Diffuse/Object.frag：ambient+diffuse（无 specular）。
// lightPos=vec4Pool[2]、lightColor=vec4Pool[3]、objectColor=vec4Pool[4]。
#include "../../_uniform_block.hlsli"

struct PSIn {
    float4 sv : SV_Position;
    float4 normal : TEXCOORD0;
    float4 fragPos : TEXCOORD1;
};

float4 PSMain(PSIn i) : SV_Target {
    // TEMP PROBE: explicit 3D normalize + 3D dot
    float3 n3 = normalize(i.normal.xyz);
    float3 dv3 = gVec4Pool[2].xyz - i.fragPos.xyz;
    float diff = saturate(dot(n3, normalize(dv3)));
    return float4(diff.xxx * gVec4Pool[3].xyz, 1.0);

    /* TEMP PROBE: original disabled
    // ambient
    float ambientStrength = 0.3;
    float4 ambient = ambientStrength * gVec4Pool[3];

    // diffuse
    float4 norm = normalize(i.normal);
    float4 lightDir = normalize(gVec4Pool[2] - i.fragPos);
    float diff2 = max(dot(norm, lightDir), 0.0);
    float4 diffuse = diff2 * gVec4Pool[3];

    float4 result = (ambient + diffuse) * gVec4Pool[4];
    return result;
    */
}
