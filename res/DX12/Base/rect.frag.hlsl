// 对应 res/GL/Base/rect.frag：直接输出插值顶点色。
float4 PSMain(float4 col : TEXCOORD0) : SV_Target {
    return col;
}
