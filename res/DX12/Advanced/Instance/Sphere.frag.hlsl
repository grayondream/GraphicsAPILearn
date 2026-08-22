// 对应 res/GL/Advanced/Instance/Sphere.fs。
struct PSIn {
    float4 sv : SV_Position;
    float4 color : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    return i.color;
}
