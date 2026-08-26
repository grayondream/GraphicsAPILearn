// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T ps_5_0 编译，入口 PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/Geometry/NormalLine.fs：法线线固定黄色。
float4 PSMain() : SV_Target {
    return float4(1.0, 1.0, 0.0, 1.0);
}
