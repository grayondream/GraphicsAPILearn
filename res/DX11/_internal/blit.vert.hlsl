// DX11/SM5.0 镜像（自 res/DX12 同名拷贝，fxc /T vs_5_0|ps_5_0 编译，入口 VSMain/PSMain；
// 逐文件核对无 SM6 专属语法）。
// 全屏大三角形（SV_VertexID 生成，无顶点缓冲/无输入布局）：
// 顶点 (-1,-1) (3,-1) (-1,3)；uv 覆盖 [0,2]，视口裁剪后恰为 [0,1]。
// D3D NDC y 向上：正高度视口 sy=Y+(1-y_ndc)*H/2 把 y=+1 映到行 0（顶），
// 故 uv.y 必须取 0.5-p.y*0.5 才能使 dst 行 r 读 src 行 r——恒等映射不翻转
// （mip 降采样与 RT↔RT blit 均保持源方向；与 VK vkCmdBlitImage 行为对齐）。
// 【历史】曾误按 "D3D NDC y-down" 写成 uv=pos*0.5+0.5：每级 mip/每次 color blit
// 均被垂直翻转，奇数级 mip 内容颠倒（LinearMipLinear 缩小时与基级混叠出重影）。
struct VSOut {
    float4 sv : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(uint vid : SV_VertexID) {
    VSOut o;
    float2 p = float2(vid == 1 ? 3.0f : -1.0f, vid == 2 ? 3.0f : -1.0f);
    o.sv = float4(p, 0.0f, 1.0f);
    o.uv = float2(p.x * 0.5f + 0.5f, 0.5f - p.y * 0.5f);
    return o;
}
