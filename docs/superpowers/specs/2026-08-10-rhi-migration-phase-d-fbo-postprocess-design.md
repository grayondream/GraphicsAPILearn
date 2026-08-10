# 设计文档：子项目 B 计划D — Hdr/Bloom/Defer 3 App 迁移到 RHI

日期：2026-08-10
状态：待评审

## 1. 背景与目标

子项目 B 主体为「46 App 全量迁移到 RHI」。已完成：前置补全、计划A（RHI 能力）、计划B（SimpleLight 9 App，确立 RhiGeometry + 双管线模板）、计划C（BlinnPhong/Gamma/NormalMap/ParallaxMap 4 App，新增 RhiGeometry::CreateFromArray）。

本计划（计划D）迁移 Light/Advanced 下 **3 个后处理/FBO App**：**Hdr、Bloom、Defer**。这是首个大量依赖 **帧缓冲对象（render-to-texture）** 的批次，验证 RHI 的 IRenderTarget/多附件能力，并补齐两个缺口：深度-only blit、附件采样参数可配。

### 范围：3 个 App + 2 个 RHI 扩展

| App | 场景 | 关键 FBO | 顶点形式 |
|---|---|---|---|
| Hdr | 立方体隧道 + HDR 曝光后处理 | 1×RGBA16F 颜色 + 深度 RBO | 手写 Cube（36 顶点 pos/normal/uv）+ 全屏 quad |
| Bloom | 立方体群 + 泛光 | 2×RGBA16F 颜色（ClampToEdge）+ 深度 RBO + 2 个 pingpong FBO | Cube/Plane（Shape）+ 全屏 quad |
| Defer | 延迟着色（GBuffer） | 3 附件（gPosition RGBA16F / gNormal RGBA16F / gAlbedoSpec RGBA8，Nearest）+ 深度 RBO | Cube/Plane（Shape）+ 全屏 quad |

全部继承 `GLCameraBaseApp`（已 RHI 化）。shader 为 `.vs/.fs`。

## 2. 关键架构变更

### 2.1 RHI 扩展 A：`blitFramebuffer` 支持缓冲位掩码（Defer 必需）

Defer 需把 GBuffer 的深度缓冲**仅拷贝**到默认 framebuffer（`glBlitFramebuffer(... GL_DEPTH_BUFFER_BIT, GL_NEAREST)`），供后续 lightbox 渲染正确深度测试。现 `IRenderer::blitFramebuffer(src, dst)` 固定 `GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT`，直接调用会覆盖默认 framebuffer 的颜色。

**方案（已确认：枚举掩码参数）**：
- 新增枚举 `enum class BlitMask : uint8_t { None=0, Color=1, Depth=2 }`（放 `rhi/core/Common.hpp`）。
- `IRenderer::blitFramebuffer(src, dst, mask = BlitMask::Color | BlitMask::Depth)`——默认值保持现有行为，向后兼容。
- GL 后端按 mask 拼接 `GL_COLOR_BUFFER_BIT` / `GL_DEPTH_BUFFER_BIT`。
- 语义明确，Vulkan 后端可对应映射。已有调用（若有）无需改动。

### 2.2b RHI 扩展 A2：`bindTexture` 支持 raw 指针重载（后处理采样必需）

`IRenderTarget::colorTexture2D(attachment)` 返回 **raw `ITexture2D*`**（其所有权在 render target 内部，用 `std::unique_ptr<GLTexture2D>` 持有），而 `IRenderer::bindTexture` 现只接受 `shared_ptr<ITexture2D>`。三个 App 后处理阶段都需要把 FBO 的颜色纹理采样给全屏 quad shader（Hdr 的 hdrBuffer、Bloom 的 scene/bloomBlur、Defer 的 gPosition/gNormal/gAlbedoSpec），无法直接绑定 raw 指针。

**方案**：给 `IRenderer` 增加 raw 指针重载 `bindTexture(rhi::ITexture2D*, unsigned int unit = 0)`（与 `ITexture3D*` 同理）。GL 后端实现即 `if (tex) tex->bind(unit)`（与 shared_ptr 版一致，GLBackend 内部都调用 `texture->bind(unit)`）。这样 App 直接 `renderer()->bindTexture(fbo->colorTexture2D(0), 0)`。不改现有 shared_ptr 版，向后兼容。

### 2.2 RHI 扩展 B：`FramebufferAttachment` 增加采样参数（Bloom/Defer 必需）

- Bloom 的 2 个颜色纹理必须 `ClampToEdge`（blur 越界否则错误重复采样）+ Linear。
- Defer 的 gPosition/gNormal 必须 `Nearest`（避免 GBuffer 线性插值模糊）。
- 现 `FramebufferAttachment` 仅有 type/format/external/samples，GLRenderTarget::create 固定 Linear + 默认 wrap。

**方案（已确认：加 filter/wrap 字段）**：
- `FramebufferAttachment` 增加 `minFilter/magFilter`（`TextureFilter`，默认 Linear）与 `wrapS/wrapT`（`TextureWrap`，默认 ClampToEdge）。
- `GLRenderTarget::create(desc)` 用这些字段替换硬编码的采样参数（颜色与深度附件均应用）。
- 深度附件使用 Nearest + ClampToEdge（与现行为一致）。
- Bloom 显式传 `wrapS=wrapT=ClampToEdge, minFilter=magFilter=Linear`；Defer 传 `minFilter=magFilter=Nearest`。

### 2.3 顶点承载：全屏 quad 用 `CreateFromArray`，Cube/Plane 复用 `Create`

- 三个 App 的**全屏 quad** 均为 pos+uv 交错（5 float/顶点，4 顶点，`TriangleStrip`）。用 `RhiGeometry::CreateFromArray` + 自定义 `VertexLayout`（pos@0, uv@1, stride 20）。RhiGeometry 的 CreateFromArray 单 VBO binding 0，正好适用。
- **Hdr 的 Cube**：36 顶点手写交错数组（pos/normal/uv，8 float/顶点，`TriangleList`，drawArrays 36）。用 `CreateFromArray`。
- **Bloom/Defer 的 Cube/Plane**：复用已迁移的 Shape 类，用 `RhiGeometry::Create(shape, ...)`（Cube indexed，Plane drawArrays 6），与 SimpleLight 系列一致。
- `PrimitiveType::TriangleStrip` 已由 RHI 支持（GL 后端已映射）。

### 2.4 FBO 尺寸来源

原 GL 代码用 `GetWindowWidth()/GetWindowHeight()` 全局常量。迁移后用 `m_window->getProperties()`（GLCameraBaseApp 既有成员，与计划C 一致）获取窗口宽高，据此创建 FBO。

## 3. 各 App 迁移要点

### 3.1 Hdr（T2）
- FBO：`IRenderTarget::create(FramebufferDesc{width,height, attachments:{ Color(RGBA16F), Depth(Depth24Stencil8 或 Depth32F) }})`。
- 物体渲染：setRenderTarget(fbo) → setPipeline(lighting) → 绑定 wood.png → drawArrays(36,0) → setRenderTarget(nullptr)。
- 后处理：setPipeline(hdr) → 绑定 fbo->colorTexture2D(0) → 全屏 quad → draw(4,0)（TriangleStrip）。uniform：hdr(bool)、exposure(float)。
- 光源：4 个点光 pos/color，数组 uniform `lights[i].Position/Color`（照计划C Gamma 的数组 uniform 方式）。

### 3.2 Bloom（T3）
- HDR FBO：2×RGBA16F 颜色附件（ClampToEdge）+ 深度 RBO。
- pingpong：2 个 IRenderTarget（各 1×RGBA16F，ClampToEdge），blur 10 次交替绑定，从 `colorTexture2D(1)` 采样、绑定目标 FBO、全屏 quad。
- 最终合成：2 个纹理（scene=colorBuffers[0], bloomBlur=pingpong[1]）→ 全屏 quad。
- 4 个管线：bloom/light/blur/final（照原代码）。

### 3.3 Defer（T4）
- GBuffer：3 附件（gPosition RGBA16F Nearest / gNormal RGBA16F Nearest / gAlbedoSpec RGBA8 Nearest）+ 深度 RBO。
- 几何 pass：setRenderTarget(gBuffer) → 3 附件 → 绘制 Cube 群 + Plane。
- Light pass：setRenderTarget(nullptr) → 绑定 3 个 gBuffer 纹理到 unit 0/1/2 → 全屏 quad（延迟光照）。
- **深度 blit**：`blitFramebuffer(gBuffer, nullptr, BlitMask::Depth)` 拷深度到默认 FBO。
- LightBox pass：绘制代表光源的小立方体（依赖上面拷入的深度做遮挡）。
- 注意：Light.fs 采样 gPosition 等需绑定从 `colorTexture2D(0/1/2)` 拿到的纹理。

## 4. 错误处理

- FBO 创建失败：`ErrorHandle::ExitIfFailed`（沿用现 GL 代码的退出策略），日志用 `SPDLOG_INFO`/`ErrorHandle`。
- `colorTexture2D(attachment)` 返回 null 时（附件不存在）需防护；本批次附件索引均在有效范围。

## 5. 测试与回归

- 每个任务完成后：`./scripts/build_run.sh build` + `./scripts/run.sh all -b gl -d 1`，须 **46/46 OK**。
- 各自 App 单独运行 `./scripts/run.sh <name> -b gl -d 1` 确认可运行。
- 回归红线不变。

## 6. 任务拆分

| 任务 | 内容 | 提交 | 验证 |
|---|---|---|---|
| T1 | RHI 扩展：BlitMask 枚举 + blitFramebuffer 掩码 + FramebufferAttachment filter/wrap + bindTexture raw 指针重载 + GL 后端实现 | feat(rhi) | build + 46/46 |
| T2 | Hdr 迁移 | refactor(app) | build + 46/46 + Hdr 单跑 |
| T3 | Bloom 迁移 | refactor(app) | build + 46/46 + Bloom 单跑 |
| T4 | Defer 迁移 | refactor(app) | build + 46/46 + Defer 单跑 |

采用 Subagent-Driven 执行（同计划C）。ledger 放 `.superpowers/sdd/2026-08-10-rhi-migration-phase-d-fbo-postprocess/`。

## 7. 技术债/后续

- Shadow 系列（ShadowMap/Shadow/PointLightShadow）涉及**深度 cubemap、深度-only 渲染、mip** 等更深缺口，留到后续批次。
- SSAO 涉及 RGBA32F noise 等，后续批次处理。
- 本批次沿用现纹理采样参数扩展，为 Shadow 的深度纹理可采样（depthTexture2D）铺路（后续扩展）。
