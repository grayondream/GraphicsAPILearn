// 对应 res/GL/Light/Material/Object.frag（材质槽位光照）：
// material.ambient→vec4Pool[7]、diffuse→vec4Pool[8]、specular→vec4Pool[9]、shininess→floatPool[0]。
// GLSL 死变量 viewValue / 死接口 objOriginColor 未搬运。
#include "../../_uniform_block.hlsli"

struct PSIn {
    float4 sv : SV_Position;
    float4 normal : TEXCOORD0;
    float4 fragPos : TEXCOORD1;
};

float4 PSMain(PSIn i) : SV_Target {
    // ambient
    float4 ambient = gLights[0].ambient * gVec4Pool[7];

    // diffuse
    float4 norm = normalize(i.normal);
    float4 lightDir = normalize(gLights[0].position - i.fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    float4 diffuse = gLights[0].diffuse * (diff * gVec4Pool[8]);

    // specular
    float4 viewDir = normalize(gVec4Pool[0] - i.fragPos);
    float4 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), FPOOL(0));
    float4 specular = gLights[0].specular * (spec * gVec4Pool[9]);

    // combination
    float4 result = (ambient + diffuse + specular);
    return result;
}
