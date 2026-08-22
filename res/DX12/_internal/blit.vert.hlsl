// 全屏大三角形（SV_VertexID 生成，无顶点缓冲/无输入布局）：
// 顶点 (-1,-1) (3,-1) (-1,3)；uv = pos*0.5+0.5 覆盖 [0,2]，视口裁剪后恰为 [0,1]。
// D3D NDC y 向下：y=-1 为屏幕上沿，uv.y=0 对应纹理行 0，恒等映射不翻转
// （mip 降采样与 RT↔RT blit 均保持源方向）。
// 内部专用 root signature：param0 = SRV 表 t0、静态采样器 s0 = Linear+Clamp。
struct VSOut {
    float4 sv : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(uint vid : SV_VertexID) {
    VSOut o;
    float2 p = float2(vid == 1 ? 3.0f : -1.0f, vid == 2 ? 3.0f : -1.0f);
    o.sv = float4(p, 0.0f, 1.0f);
    o.uv = p * 0.5f + 0.5f;
    return o;
}
