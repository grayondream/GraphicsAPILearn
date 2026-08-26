// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T ps_5_0 编译，入口 PSMain；
// 逐文件核对无 SM6 专属语法）。
// 对应 res/GL/Advanced/Geometry/Base.fs：GS 输出颜色直传。
struct PSIn {
    float4 sv : SV_Position;
    float3 fColor : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_Target {
    return float4(i.fColor, 1.0);
}
