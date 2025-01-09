
struct vin
{
    float4 position : POSITION;   // 顶点位置
    float4 color : COLOR;         // 顶点颜色
    float2 texCoords : TEXCOORD0; // 纹理坐标
};

struct vout
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 texCoords : TEXCOORD0;  // 纹理坐标
};

vout vs_main(vin input)
{
    vout output;
    output.position = input.position;
    output.color = input.color;
    output.texCoords = input.texCoords;
    return output;
}

Texture2D gtexture : register(t0); // 绑定到纹理单元 t0
SamplerState gsamper : register(s0); // 绑定到采样器单元 s0

struct PSInput{
    float2 texCoords : TEXCOORD0; // 纹理坐标
};

float4 ps_main(PSInput input) : SV_TARGET
{
    return gtexture.Sample(gsamper, input.texCoords);
}