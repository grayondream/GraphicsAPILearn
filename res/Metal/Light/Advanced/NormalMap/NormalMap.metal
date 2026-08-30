#include <metal_stdlib>
using namespace metal;
// __MAT_HELPERS__ (auto-added: MSL lacks inverse()/mat4(mat3))
float3x3 mat3Inverse(float3x3 m) {
    float a00 = m[0][0], a01 = m[0][1], a02 = m[0][2];
    float a10 = m[1][0], a11 = m[1][1], a12 = m[1][2];
    float a20 = m[2][0], a21 = m[2][1], a22 = m[2][2];
    float b01 =  a22*a11 - a12*a21;
    float b11 = -a22*a10 + a12*a20;
    float b21 =  a21*a10 - a11*a20;
    float det = a00*b01 + a01*b11 + a02*b21;
    float id = 1.0 / det;
    return float3x3(
        b01*id, (-a22*a01 + a02*a21)*id, ( a12*a01 - a02*a11)*id,
        b11*id, ( a22*a00 - a02*a20)*id, (-a12*a00 + a02*a10)*id,
        b21*id, (-a21*a00 + a01*a20)*id, ( a11*a00 - a01*a10)*id
    );
}
float4x4 mat4FromMat3(float3x3 m) {
    return float4x4(float4(m[0], 0.0), float4(m[1], 0.0), float4(m[2], 0.0), float4(0.0, 0.0, 0.0, 1.0));
}

struct ULight {
    float4 position;
    float4 direction;
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float4 params;
};

struct UniformBlock {
    float4x4 projection;
    float4x4 view;
    float4x4 model;
    float4x4 normalMatrix;
    float4x4 viewModel;
    float4x4 extraMat4[14];
    float4 vec4Pool[64];
    float4 vec3Pool[64];
    float floatPool[64];
    ULight lights[256];
};

struct VertexIn {
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float2 textureCoord [[attribute(2)]];
    float3 tangent [[attribute(3)]];
    float3 bitangent [[attribute(4)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 FragPos;
    float2 TexCoords;
    float3 TangentLightPos;
    float3 TangentViewPos;
    float3 TangentFragPos;
    float3 aNormal;
};

vertex VertexOut NormalMap_NormalMap_vertex(VertexIn in [[stage_in]],
                                           constant UniformBlock& ubo [[buffer(8)]]) {
    VertexOut out;
    out.FragPos = float3(ubo.model * float4(in.pos, 1.0));   
    out.TexCoords = in.textureCoord;
    
    float3x3 normalMatrix = transpose(mat3Inverse(float3x3(ubo.model[0].xyz, ubo.model[1].xyz, ubo.model[2].xyz)));
    float3 T = normalize(normalMatrix * in.tangent);
    float3 N = normalize(normalMatrix * in.normal);
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T);
    
    float3x3 TBN = transpose(float3x3(T, B, N));    
    out.TangentLightPos = TBN * ubo.vec4Pool[2].xyz;
    out.TangentViewPos  = TBN * ubo.vec4Pool[0].xyz;
    out.TangentFragPos  = TBN * out.FragPos;
    out.aNormal = in.normal;
    out.position = ubo.projection * ubo.view * ubo.model * float4(in.pos, 1.0);
    return out;
}

fragment float4 NormalMap_NormalMap_fragment(VertexOut in [[stage_in]],
                                            constant UniformBlock& ubo [[buffer(8)]],
                                            texture2d<float> diffuseMap [[texture(0)]],
                                            texture2d<float> normalMap [[texture(1)]],
                                            sampler diffuseSampler [[sampler(0)]],
                                            sampler normalSampler [[sampler(1)]]) {

     // obtain normal from normal map in range [0,1]
    float3 normal = in.aNormal;//texture(normalMap, in.TexCoords).rgb;
    if(ubo.floatPool[22] > 0.5){
        normal = normalMap.sample(normalSampler, in.TexCoords).rgb;
    }
    // transform normal vector to range [-1,1]
    normal = normalize(normal * 2.0 - 1.0);  // this normal is in tangent space
   
    // get diffuse color
    float3 color = diffuseMap.sample(diffuseSampler, in.TexCoords).rgb;
    // ambient
    float3 ambient = 0.1 * color;
    // diffuse
    float3 lightDir = normalize(in.TangentLightPos - in.TangentFragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    float3 diffuse = diff * color;
    // specular
    float3 viewDir = normalize(in.TangentViewPos - in.TangentFragPos);
    float3 reflectDir = reflect(-lightDir, normal);
    float3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    float3 specular = float3(0.2) * spec;
    return float4(ambient + diffuse + specular, 1.0);

}