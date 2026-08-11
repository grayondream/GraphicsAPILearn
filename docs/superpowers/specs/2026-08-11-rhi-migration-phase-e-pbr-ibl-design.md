# 计划E：PBR/IBL 批次迁移到 RHI

- 日期：2026-08-11
- 状态：已批准（brainstorming 评审通过）
- 目标：迁移 5 个 PBR/IBL 示例 App 至 RHI 抽象层，脱离原生 OpenGL（GLProgram/glad/GLImageTexture2D/GLCube/GLPlane），并补充 1 个 RHI 能力缺口。

## 1. 范围与背景

继计划 B（SimpleLight 9 App）、计划 C（BlinnPhong/Gamma/NormalMap/ParallaxMap 4 App）、计划 D（Hdr/Bloom/Defer 3 App + FBO 后处理扩展）之后，本批次处理位于 `src/app/GL/Advanced/PBR/` 的 5 个基于物理渲染（PBR）及基于图像的光照（IBL）示例。

| AppType | 源文件 | 复杂度 |
|---|---|---|
| PBR_Base | GLPBRBaseApp | 简单 |
| PBR_Texture | GLPBRTextureApp | 简单 |
| PBR_IBL_Irradiance_Conversion | GLIBLIrradianceConversionApp | 中 |
| PBR_IBL_Irradiance | GLIBLIrradianceApp | 中 |
| PBR_IBL_Specular | GLIBLSpecularApp | 难 |

这是首个批量涉及 **cubemap 渲染到渲染目标（render-to-cubemap-face）** 和 **多步骤 GPU 预处理（renderBeforeLoop）** 的批次。

### 1.1 各 App 依赖

- **PBR_Base**：sphere 几何；默认帧缓冲；单 PBR shader；4 个光源。
- **PBR_Texture**：sphere 几何 + 5 张 2D 纹理（albedo/roughness/metallic/ao/normal 绑定 unit 1-5）。
- **PBR_IBL_Irradiance_Conversion（IC）**：HDR(equirectangular)→cubemap 转换，渲染到 cubemap 6 面（512×512，RGB16F）；背景用 cubemap；PBR 直接光。
- **PBR_IBL_Irradiance**：IC 全部 + 额外生成 irradiance cubemap（32×32 RGB16F）；PBR 用 irradiance 作为间接漫反射。
- **PBR_IBL_Specular**：Irradiance 全部 + prefilter cubemap（128×128，5 级 mip，渲染到指定 mip 级别）+ BRDF LUT 2D 纹理（512×512 RG16F）；PBR 结合 irradiance + prefilter + BRDF LUT。

### 1.2 生命周期：renderBeforeLoop

IC / Irradiance / Specular 在 `initApp` 阶段于 `renderBeforeLoop` 执行 GPU 预处理（每帧不重复，仅一次），并把结果 cubemap / 2D 纹理作为成员纹理用于每帧 `drawScene`。

## 2. RHI 能力现状与缺口

### 2.1 已具备（无需改动）

| 能力 | RHI 接口 / 位置 |
|---|---|
| 渲染到 cubemap 面（mip 0） | `IRenderTarget::attachCubeFace(ITexture3D*, int face)` |
| cubemap 存储分配 + mipmap 生成 | `ITexture3D::createEmpty(TextureDesc, w, h)`（GLTexture3D.cpp:58 `generateMipmap`） |
| 渲染到 2D 纹理（BRDF LUT） | `IRenderTarget` + 2D Color attachment |
| 视口控制 | `IRenderer::setViewport` |
| HDR 加载 | `RhiImage::Load2DHDR` |
| 深度测试 / 深度函数 | `IPipeline::setDepthTest` / `setDepthFunc` |
| 多 binding 顶点缓冲 / 图元拓扑 | 已有 |

### 2.2 缺口与扩展（本批次 RHI 改动）

**缺口 E-1：`attachCubeFace` 支持指定 mip 级别。**

- 现状：`IRenderTarget::attachCubeFace(ITexture3D* cube, int face)` 在 GL 实现里 `glFramebufferTexture2D(..., mip=0)` 硬编码 0（GLRenderTarget.cpp:120-127）。
- 需要：IBL_Specular 的 prefilter 阶段渲染到 cubemap mip 1-4（`glFramebufferTexture2D(GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, m_prefilterMap, mip)`）。
- 改动：接口与 GL 实现均加 `int mip = 0` 参数（默认 0，向后兼容）。

**缺口 E-2：`GL_TEXTURE_CUBE_MAP_SEAMLESS` 未启用。**

- 现状：GL 后端任何位置均未设置 seamless cubemap 采样。
- 需要：IBL_Specular 显式 `glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS)`（GL 3.2+ 默认关闭，避免 cubemap 面间接缝）。
- 改动：在 GL 后端 `IRenderer::init`（GLBackend.cpp:27）末尾加 `glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS)`（GL 3.2+ 默认关闭，全局一次启用，避免 cubemap 面间接缝）。`BackendCapabilities` 已有接口但本批次不改其字段（可后续记入）。

其余（BRDF LUT 渲染、prefilter 多 mip 尺寸视口、cubemap mipmap 生成）均不额外改 RHI。

## 3. 任务拆分（按依赖分层）

| 任务 | 内容 | 主要 RHI 依赖 |
|---|---|---|
| T1 | RHI 扩展：attachCubeFace 加 mip 参数 + seamless cubemap | 后端 |
| T2 | 迁移 PBR_Base | 仅几何 |
| T3 | 迁移 PBR_Texture | 几何 + 2D 纹理 |
| T4 | 迁移 PBR_IBL_Irradiance_Conversion | 依赖 T1 |
| T5 | 迁移 PBR_IBL_Irradiance | 依赖 T1、T4 |
| T6 | 迁移 PBR_IBL_Specular | 依赖 T1、T5（prefilter mip + BRDF LUT） |

T2/T3 无 RHI 缺口依赖，可并行于 T1；T4/T5/T6 依赖 T1。SDD 执行时按 T1→T2→T3→T4→T5→T6 顺序串行（或 T1 完成后 T2/T3 并行）。

## 4. 实现要点

### 4.1 几何与几何上传

- 球体：`GLSphere : Sphere`，用 `RhiGeometry::Create(renderer, sphere, useUv=true, useNormal=true, useIndex=true)`。
- 立方体（cubemap 背景 / capture 用）：`GLPlane`/`GLCube` 迁移为 `RhiGeometry::Create(renderer, cube, useIndex=true)`。
- PBR/CUBE/Brdf shader 均统一 `pos@0 / inColor@1 / aTexCoords@2 / normal@3`（已核对 Base/PBR.vs、IBL_IC/CUBE.vs、IBL_Specular/Brdf.vs）。这匹配 `RhiGeometry::Create` 默认 Layout `{uvLocation:2, normalLocation:3}`，无需自定义 layout。
- BRDF LUT 全屏 quad：`CreateFromArray`（pos3+uv2，stride 20，pos@0/uv@1），**管线设 `TriangleStrip`**（沿用计划 D TriangleStrip 教训）。

### 4.2 cubemap 与渲染到 cubemap 面

cubemap 创建用 `createEmpty(TextureDesc{ format, wrapS/T/R=ClampToEdge, minFilter, magFilter, generateMipmap })`。

**capture 到 cubemap（IC / Irradiance / Specular 共用模式）**：
```
RT = renderer()->createRenderTarget();
RT->create(FramebufferDesc{ width:512, height:512, attachments:[Depth24Stencil8 depth] });
for face in 0..5:
    renderer()->setViewport({0,0,size,size});
    RT->attachCubeFace(cube, face);      // mip 0
    renderer()->setRenderTarget(RT);
    clearColor(...);
    renderCube(...);
renderer()->setRenderTarget(nullptr);
renderer()->setViewport(window size);
```

**prefilter（Specular）**：
```
for mip in 0..4:
    mipW = mipH = 128 * pow(0.5, mip);
    renderer()->setViewport({0,0,mipW,mipH});
    RT->attachCubeFace(prefilter, face, mip);   // 缺口 E-1
    ...renderCube...
```

**BRDF LUT**：`createEmpty(TextureDesc{ RG16F, 512×512 })` + `IRenderTarget` 2D attachment，渲染 quad。

### 4.3 状态与管线

- 深度：`setDepthTest(true)`；capture 用 `setDepthFunc(CompareFunc::LessEqual)`（原 `glDepthFunc(GL_LEQUAL)`，因 cubemap 面共用立方体绘制需要 equal pass）。
- 每 mip 视口随尺寸变化，绘制后恢复窗口视口。
- 颜色附件 / cubemap 纹理绑定：用 `renderer()->bindTexture(ITexture3D*, unit)`（cubemap）与 `bindTexture(ITexture2D*, unit)`。
- HDR cubemap 数据：`RhiImage::Load2DHDR` 得到 2D float 纹理，供 CUBE.fs（equirectangularMap）采样转换到 cubemap。

### 4.4 静态辅助函数

保留 file-static：`GetCaptureViews()`、`GetLightPosAndColor()`、`GenreateObjPos()`、`renderCube`、`renderSphere`（与已迁移 App 一致，不改为成员）。

## 5. 数据流（IBL_Specular 示例，完整预处理链）

```
Load2DHDR(newport_loft.hdr)
   → CUBE pass（equirect→envCubemap, 512, 6面, mip0）[T4/T5/T6 共用]
   → createEmpty(irradianceMap, 32) + 6面渲染 [T5]
   → createEmpty(prefilterMap, 128, generateMipmap) + 5 mip × 6面渲染 [T6, 缺口E-1]
   → createEmpty(brdfLUT, 512 2D RG16F) + quad 渲染 [T6]
每帧 drawScene: PBR 采样 envCubemap背景 + irradiance + prefilter + brdfLUT
```

## 6. 验证与测试

- 无独立单元测试框架；依赖回归红线 + 单 App 冒烟。
- **每任务**：
  - `./scripts/build_run.sh build` 编译通过。
  - `./scripts/run.sh all -b gl -d 1` 全量 46/46 OK（无回归）。
  - 单 App 冒烟：`./scripts/run.sh all -a <AppName> -b gl -d 1` 无崩溃、日志正常。
    - IBL 相关 App（IC/Irradiance/Specular）在 init 阶段执行 GPU 预处理（HDR→cubemap 等），单跑确认无崩溃、无 GL 错误。
- **迁移后红线**：46/46 OK。
- **已知限制**：run.sh 的 OK 仅表示 exit 0 + 无 [error] 日志，非视觉金样比对；IBL 视觉等价性依赖 GPU 预处理正确性，由代码审查 + 单 App 冒烟保证（已在计划 D 记录该盲区）。

## 7. 技术债与后续

- 本批次不改 shininess 类型化（I-2 仍在 Vulkan 前待办）。
- 本批次完成后，剩余未迁移：Shadow×3（PointLightShadow/Shadow/ShadowMap）+ SSAO + SimpleGemotery。
- 后续批次规划不变：Shadow×3/SSAO → 收尾去 GL 前缀重命名。

## 8. 提交策略

与既有约定一致，提交直接落 `develop` 分支，每个 App 迁移一个提交，RHI 扩展一个提交，提交信息带 `refactor(app):` / `feat(rhi):` 前缀。
