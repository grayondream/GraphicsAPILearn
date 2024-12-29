cbuffer ConstantBuffer : register(b0) {
    float4x4 mvp;
};

struct vin{
    float4 position : POSITION;
    float4 color : COLOR;
};

struct vout{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

vout vs_main(vin input){
    vout output;

    output.position = mul(input.position, mvp);
    output.color = input.color;
    return output;
}

struct pin {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float4 ps_main(pin input) : SV_TARGET{
    return input.color;
}