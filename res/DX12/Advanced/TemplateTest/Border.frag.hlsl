// 对应 res/GL/Advanced/TemplateTest/Border.frag：模板描边纯色输出（无插值器消费）。
float4 PSMain() : SV_Target {
    return float4(1.0, 0.0, 0.0, 1.0);   // 确保输出明显的边框颜色（例如红色）
}
