#include <metal_stdlib>
using namespace metal;

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
    float3 aPos [[attribute(0)]];
    float3 aNormal [[attribute(1)]];
    float2 aTexCoords [[attribute(2)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 TexCoords;
};

vertex VertexOut Instance_Rock_vertex(VertexIn in [[stage_in]],
                                      constant UniformBlock& ubo [[buffer(0)]],
                                      const device float4x4* instanceMatrix [[buffer(1)]],
                                      uint instanceID [[instance_id]]) {
    VertexOut out;
    float radiuse = 20.0;
    out.TexCoords = in.aTexCoords;
    
    float angle = -ubo.floatPool[10] / 10;
    float xOffset = radiuse * cos(angle);
    float zOffset = radiuse * sin(angle);
    
    float4x4 rotationMatrix = float4x4(
        float4(cos(angle), 0.0, sin(angle), 0.0),
        float4(0.0, 1.0, 0.0, 0.0),
        float4(-sin(angle), 0.0, cos(angle), 0.0),
        float4(0.0, 0.0, 0.0, 1.0)
    );
    
    float4x4 translationMatrix = float4x4(1.0);
    translationMatrix[3] = float4(ubo.vec4Pool[45].xyz, 1.0);
    
    float4x4 newInstanceMatrix = translationMatrix * rotationMatrix * instanceMatrix[instanceID];
    
    out.position = ubo.projection * ubo.view * newInstanceMatrix * float4(in.aPos, 1.0);
    return out;
}

fragment float4 Instance_Rock_fragment(VertexOut in [[stage_in]],
                                       constant UniformBlock& ubo [[buffer(0)]],
                                       texture2d<float> texture_diffuse1 [[texture(0)]],
                                       sampler smp [[sampler(0)]]) {
    return texture_diffuse1.sample(smp, in.TexCoords);
}