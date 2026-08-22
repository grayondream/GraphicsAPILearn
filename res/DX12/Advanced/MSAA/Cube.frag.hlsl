// 对应 res/GL/Advanced/MSAA/Cube.fs：MSAA 内层渲染，固定输出纯绿
// （fragColor/textureCoord 死插值器未搬运）。
float4 PSMain() : SV_Target {
    return float4(0.0, 1.0, 0.0, 1.0);
}
