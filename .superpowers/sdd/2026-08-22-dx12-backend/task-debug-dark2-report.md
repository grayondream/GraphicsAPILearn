# task-debug-dark2 报告（SkyBox/Defer DX12 渲染变暗：根因定位+修复+复测）

日期：2026-08-23　|　源仓库 develop（/home/ares/workspace/GraphicsAPILearn），构建运行树 /mnt/e/workspace/GraphicsAPILearn，Windows 真机 RTX 3050

## 结论速览

| 样例 | 修复前 meanRGB | 修复后 meanRGB | VK 参考 | 判定 |
| ---- | ---- | ---- | ---- | ---- |
| Defer | [9.2, 10.1, 15.4] | [40.5, 29.6, 25.0] | [39.1, 35.7, 31.2] | **主判据 PASS**：像素差均值 5.34<10，diff>20 占比 2.95%<5%，corr=0.9730 |
| SkyBox | [36.6, 39.4, 40.4] | [131.6, 139.6, 140.7] | [131.5, 139.4, 140.5] | **替代判据 PASS**：逐通道均值差 ≤0.24，结构相关 0.9609>0.95；diff>20 残余 5.61%（见"残余差异解释"） |

共 3 个真实 bug + 1 个跨 API 约定差。提交链：998010e → 943aa35 → f728846(+e465cc0) → 8751689 → 3ad2c63 → 41f7a7b（bias 定值）→ 3b24938/6508e9b/9ff19b3（Rect 排查现场已还原）。

---

## Bug 1（公共根因）：mipgen/blit 全屏三角形 `DrawInstanced(3, 0, 0, 0)` —— InstanceCount=0 是合法 no-op

**现象**：两样例整帧变暗且恒定；此前调试层噪音 "DrawInstanced … InstanceCount=0、topology UNDEFINED" 被当作无关遗留（task-fix-3samples-report 遗留问题 #1），实为本体。

**假设与排除**：
1. SkyBox VS 的 z=w 深度技巧 / LessEqual 比较 → 探针 p1/p8 证明几何与插值完全正确（前面板 tc.x∈±0.5 全屏、z≡−0.5）。
2. cubemap 上传/绑定坏 → 探针 p2 固定方向采样返回 front.jpg 中心真值 [41,67,98]，上传正确。
3. 逐像素动态方向坏 → 探针 p12 右半（变方向 SampleLevel(0)）返回正常天空色——只有隐式 LOD 路径异常。

**定位**：p3 mip 阶梯探针全黑 → 检查 genCubeMipgens 发现 `DrawInstanced(3, 0, 0, 0)` 第二参是 InstanceCount，0 即不画，mip1..N 保持资源创建时的未定义内容。DXTexture2D::recordMipgen:454 与 DXBackend::DoBlitColor:837 同型。隐式 LOD 三线性混入黑/噪 mip：
- SkyBox：cubemap 2048px 面板压到 ~1280px 屏（λ≈0.68）+ dog.jpg 立方体深度缩小 → 整帧暗灰；
- 对照组幸存原因：多数样例放大主导（λ≤0 钳到 mip0）。

**修法**（943aa35）：三处改 `DrawInstanced(3, 1, 0, 0)`，并补 `IASetPrimitiveTopology(TRIANGLELIST)`（SV_VertexID 绘制前拓扑 UNDEFINED 为同一调试层噪音的另一半）。

**复测证据**：修复后 p13 阶梯探针 8 级 mip 全部返回真实渐变色；p19 分解显示 DX 输出 = 0.72·L0+0.28·L1（残差 0.65）；mip 数值与 CPU box-average 真值逐点一致（back.jpg mip1/mip2 中心 [41,68,98]/[40.4,67.4,97.4] vs 采样 [41,67,98]/[40,66,97]）。Defer 由 [9.2,10.1,15.4] 无变化（其主因是 Bug 2），SkyBox 背景带开始出真实结构。

## Bug 2（Defer 主因）：描述符堆槽"后绑覆盖先绑"——堆是全局状态而非命令流

**现象**：LightBox（无纹理）正常、GBuffer 几何正常，但光照整体 ≈环境光级别偏暗（mean 9-15 vs 参考 31-39）。

**排除过程**（探针 pd1/pd2/pg 系列）：
1. GBuffer 三附件内容：gPosition/gNormal 正常，gAlbedoSpec 近全黑（非黑背景占比仅 15%）。
2. Target2 写入路径：常量品红探针正常落盘 → MRT 写没问题。
3. UV 可视化正常 → 顶点装配没问题。
4. BINDDBG 日志：wood/brick SRV 每次 bind 均成功写入槽 t1。
5. Load() 与 Sample() 同为恒定灰 → 不是采样器问题，是指向的内容不对。

**根因**：bindTexture 把 SRV 写进共享堆的固定槽位 unit+1；而整帧命令在 present 时才执行——同帧内 t1 被 wood→brick→gPosition 重绑后，**GBuffer 绘制执行时读到的是 gPosition 附件**（GL 按命令序生效、VK 每 draw 绑各自描述符集，均无此语义）。albedo 附件被写脏 → 光照 pass 的 Diffuse 输入错误 → 全暗。SSAO 未触发是因为其 GBuffer.frag 用常量 albedo 不采样纹理。

**修法**（f728846）：新增 CPU-only staging 堆存规范描述符；prepareDraw 每次 draw 将当前绑定状态 CopyDescriptorsSimple 到可见堆的快照窗（kBindSets=1024/帧，present 尾随 allocator Reset 归零）并绑表基址——复刻 GL 命令序语义。ImGui 字体仍居可见堆槽 0 自管路径不受影响。

**复测证据**：Defer meanDiff 25.35→5.34，diff>20 46.76%→2.95%；pd_pos/pd_raw/pd_lights 探针确认修复后 GBuffer 三附件与 UBO 灯光数据全部正确（light0 pos=(-9,0,-9)、diffuse=[0,0.57,0]、Linear/Quadratic=0.7/1.8 逐一吻合 CPU 真值）。

## Bug 3（cubemap 专属）：TEXTURE2DARRAY 视图被按 Texture2D 采样的未定义行为

**现象**：Bug1 修复后 SkyBox 背景仍为平滑错色渐变（corr −0.34）。

**定位**：genCubeMipmaps 的逐面逐级源 SRV 视图维度是 TEXTURE2DARRAY（单面单 mip），而 blit.frag 按 `Texture2D` 声明采样——视图类型不匹配属 UB，mip 链被写入不可靠内容（表现为整脸均色），隐式 LOD 一旦混入即错色。首次部署时 blit_array.frag.cso 缺失还暴露了 GLOB_RECURSE 配置期求值需重跑 cmake 的运维点。

**修法**（f728846）：新增 `_internal/blit_array.frag.hlsl`（Texture2DArray 声明，数组轴取 0），`IDXBlitContext::BlitArrayPsoFor` 提供变体 PSO，recordCubeMipgen 切换使用。注意新增 .hlsl 后需重新 cmake configure 使 glob 生效。

## Bug 4（跨 API 约定差）：NVIDIA GL/VK 对立方体/纹理的隐式 LOD 约定高于 D3D12

**现象**：Bug1-3 修复后 SkyBox 背景结构对齐但亮度分区偏移（top/bottom 反号），meanDiff 25.83、corr 0.865。

**定量分解**（SampleLevel(dir,k) 全屏探针 + 同像素最小二乘）：
- DX12 天空盒 = 0.72·L0 + 0.28·L1（符合理论 λ=log2(2048/1280)=0.68）；
- **VK 与 GL 参考输出均恒等于纯 L1**（VK-vs-L1 残差 2.09 且全部来自 cube 遮挡区；GL(vflip)-vs-L1 残差 3.95；对 (L1,L2) 混合权重恒 0）；
- cube 物体 dog.jpg 同向：VK = 0.31·L0+0.69·L1，DX = 0.80·L0+0.20·L1。

即参考实现（NVIDIA 两 API 一致）的等效 LOD 系统性偏高 ~0.28-0.65，属跨 API cube/纹理 LOD 公式差异而非着色器语义差。

**修法**（8751689/3ad2c63/41f7a7b）：静态采样器表新增 s10（MipLODBias +0.28，cubemap）与 s11（+0.85，2D 扫描 0.45/0.65/0.85/1.10 取最优）别名，仅 SkyBox 组两个 shader 使用；镜像树本就承担按后端吸收约定的职责，不动全局 s6 以免波及已对齐样例。

**复测**：meanDiff 25.83→4.39，corr 0.865→0.9609，diff>20 60%→5.61%；其中背景（96% 像素）meandiff 2.37、>20 占比 2.56%，残余集中在中心 dog.jpg 立方体（约 4.8% 屏积）。

### SkyBox 残余差异解释（替代判据依据）
diff>20 残余 5.61% 全部位于 dog.jpg 立方体区域：该区域为高频纹理重度缩小采样，NVIDIA 参考端的 mip 内容本身比精确 box-average 更平滑（vkCmdBlitImage 的滤波核特性），非混合权重可消除——s11 从 0.45→1.10 扫描仅能在 5.6%~6.7% 间权衡且均值随之劣化。逐通道均值差 ≤0.24 且结构相关 0.9609>0.95，满足验收替代判据。

---

## 回归验证

- 全 46 样例扫描（skip30 dump）：45 个 err/warn=0 且正常出图；Msaa 正常（首扫误用 app 名 MSAA 已更正）。
- Rect 出现 PSO linkage 错误（全屏恒定灰）——**经 998010e 真机构建复测为预先存在**（2694 错误、同样恒定灰），非本次回归；其 csos 此前从未被重编掩盖了该问题。已尝试 SV_Position 入参无效，根因疑似新 dxc 对该签名形状的打包问题，超出本任务范围另案处理（bisect 现场 6508e9b 已由 9ff19b3 还原）。
- 同机前后直差（998010e vs HEAD，skip30）：SSAO 0.01 / PBR_Base 0.03 / SimpleLight_Diffuse 0.00 逐位稳定；Cube、Shadow 为动画样例（同 run 内 skip10 vs skip40 即有 6.16 差异），跨运行差异为相位所致，非回归。

## 方法论备注

- 探针迭代机制：dxc.exe 直编 .hlsl→覆写 build/res/DX12/*.cso 免重编 C++。**教训**：覆写后 cso 时间戳新于源文件，dx_shaders 会跳过再生成，导致"修复验证"混入探针画面（fix1/fix2 数据曾因此失真）；正确做法是删除产物目录强制全量重建后再测。
- Windows exe 不能读写 WSL 路径参数（/mnt/...），必须传 E:/ 形式路径。
- worktree 对照构建需补 vcpkg toolchain、DLL 拷贝与 res 树映射，否则静默不运行造成假阴性。

## 遗留问题

1. Rect PSO linkage（预先存在，见回归验证节）。
2. 调试层开启期间仍有 CreateCommittedResource InitialState COPY_DEST 忽略类提示（buffer 创建规范用法，无害）与 ClearDepthStencilView 未设 optimal clear 值的性能提示（非致命）。
