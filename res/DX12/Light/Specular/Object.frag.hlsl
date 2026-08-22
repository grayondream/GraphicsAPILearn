// 对应 res/GL/Light/Specular/Object.frag（滑杆强度光照）：
// ambientStrength→floatPool[1]、diffuseStrength→floatPool[3]、specularStrength→floatPool[2]、
// lightPos→vec4Pool[2]、times→floatPool[10]、objectColor→vec4Pool[4]、viewPos→vec4Pool[0]、
// lightColor→vec4Pool[3]。GLSL 死变量 viewValue / 死接口 objOriginColor 未搬运。
#include "../../_uniform_block.hlsli"

struct PSIn {
    float4 sv : SV_Position;
    float4 normal : TEXCOORD0;
    float4 fragPos : TEXCOORD1;
};

float4 PSMain(PSIn i) : SV_Target {
    // ambient
    float4 ambient = FPOOL(1) * gVec4Pool[3];

    // diffuse
    float4 norm = normalize(i.normal);
    float4 lightDir = normalize(gVec4Pool[2] - i.fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    float4 diffuse = FPOOL(3) * diff * gVec4Pool[3];

    // specular
    float4 viewDir = normalize(gVec4Pool[0] - i.fragPos);
    float4 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), FPOOL(10));
    float4 specular = FPOOL(2) * spec * gVec4Pool[3];

    // combination
    float4 result = (ambient + diffuse + specular) * gVec4Pool[4];
    return result;
}
