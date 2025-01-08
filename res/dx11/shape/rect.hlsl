struct vout
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

vout vs_main(float4 position : POSITION, float4 color : COLOR)
{
    vout output;

    output.position = position;
    output.color = color;

    return output;
}


float4 ps_main(float4 position : SV_POSITION, float4 color : COLOR) : SV_TARGET
{
    return color;
}