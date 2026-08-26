// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/MSAA/Cube.fs：MSAA 内层渲染，固定输出纯绿
// （fragColor/textureCoord 死插值器未搬运）。
float4 PSMain() : SV_Target {
    return float4(0.0, 1.0, 0.0, 1.0);
}
