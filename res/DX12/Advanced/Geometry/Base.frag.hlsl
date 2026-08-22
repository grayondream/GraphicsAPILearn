// 对应 res/GL/Advanced/Geometry/Base.fs：GS 输出颜色直传。
struct PSIn {
    float4 sv : SV_Position;
    float3 fColor : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    return float4(i.fColor, 1.0);
}
