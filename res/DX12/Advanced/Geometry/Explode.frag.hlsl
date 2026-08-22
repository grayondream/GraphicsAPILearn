// 对应 res/GL/Advanced/Geometry/Explode.fs：爆炸三角颜色直传（GS 输出 float4）。
struct PSIn {
    float4 sv : SV_Position;
    float4 fColor : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    return i.fColor;
}
