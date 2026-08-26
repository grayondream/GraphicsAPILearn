// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/TemplateTest/Border.frag：模板描边纯色输出（无插值器消费）。
float4 PSMain() : SV_Target {
    return float4(1.0, 0.0, 0.0, 1.0);   // 确保输出明显的边框颜色（例如红色）
}
