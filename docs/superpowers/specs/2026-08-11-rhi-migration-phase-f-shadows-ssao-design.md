# RHI 迁移计划 F：Shadow×3 + SSAO 设计

- 日期：2026-08-11
- 状态：已批准（设计评审通过，待写实施计划）
- 前置：计划E（PBR/IBL 5 App 迁移，已合入 develop，HEAD=`67714da`）

## 1. 目标

将 4 个 App 从 native OpenGL（GLProgram/glad）迁移到 RHI，并补齐 3 个 RHI 缺口：

- `src/app/GL/Light/Advanced/GLShadowApp`（方向光阴影 + PCF + debug）
- `src/app/GL/Light/Advanced/GLShadowMapApp`（简化阴影 + 深度可视化）
- `src/app/GL/Light/Advanced/GLPointLightShadowApp`（点光源阴影：深度 cubemap + Geometry Shader）
- `src/app/GL/Light/Advanced/GLSSAOApp`（GBuffer + SSAO + Blur + Lighting，已部分 RHI 化）

项目约定（沿用自前各批次）：提交直接落 `develop`；迁移期不重命名 `GL*App` 类；回归红线 = 全量 46/46 OK；`run.sh` 冒烟等 run.sh 输出（exit0 + 无 `[error]`），非视觉金样比对。

## 2. 任务拆分（依赖分层）

| 任务 | 内容 | 类型 |
| ---- | ---- | ---- |
| T1 | RHI 扩展：深度附件 filter/wrap/borderColor + 单通道 ToGLFormat（R32F→GL_RED、RG16F→GL_RG） | RHI 扩展 |
| T2 | RHI 扩展：`IRenderTarget::attachDepthCube` 把整个深度 cubemap 挂到 `DEPTH_ATTACHMENT` | RHI 扩展 |
| T3 | 迁移 GLShadowMapApp（最简阴影） | 重构 |
| T4 | 迁移 GLShadowApp（方向光 + PCF + debug） | 重构 |
| T5 | 迁移 GLPointLightShadowApp（深度 cubemap + GS） | 重构 |
| T6 | 迁移 GLSSAOApp（GBuffer/SSAO/Blur/Light 全 RHI + noise RhiImage） | 重构 |

## 3. RHI 缺口设计

### 3.1 T1：深度附件 filter/wrap/borderColor + 单通道格式

现状：`GLRenderTarget::create(FramebufferDesc)` 的深度附件路径（GLRenderTarget.cpp:91-103）硬编码 `GL_NEAREST`/`GL_CLAMP_TO_EDGE`，无视 `FramebufferAttachment` 的 filter/wrap 字段（Phase D 技术债①）。Shadow 需 `CLAMP_TO_BORDER` + 白 border。

**改动 1：`src/rhi/core/Common.hpp` 的 `FramebufferAttachment` 增加字段**

```cpp
struct FramebufferAttachment {
    // ...现有字段...
    float borderColor[4]{0.0f, 0.0f, 0.0f, 1.0f};  // CLAMP_TO_BORDER 时生效
};
```

**改动 2：`src/rhi/gl/GLRenderTarget.cpp` 深度路径应用 a.minFilter/a.magFilter/a.wrapS/a.wrapT**，并在 wrap==ClampToBorder 时调用：

```cpp
glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, a.borderColor);
```

**改动 3：`src/rhi/gl/GLHeader.hpp` 的 `ToGLFormat` 增加单通道/双通道映射**

```cpp
inline GLenum ToGLFormat(TextureFormat f, int channels) {
    if (f == TextureFormat::Depth24Stencil8) return GL_DEPTH_STENCIL;
    if (f == TextureFormat::Depth32F) return GL_DEPTH_COMPONENT;
    if (f == TextureFormat::R32F) return GL_RED;
    if (f == TextureFormat::RG16F) return GL_RG;
    // RGBA16F etc 沿用 (channels == 4) ? GL_RGBA : GL_RGB
    ...
}
```

这是修正（原 `channels!=4→GL_RGB` 对 R32F 是 GL_RED 语义、对 RG16F 是 GL_RG 语义）。影响面：已确认 Hdr/Bloom/Defer/GBuffer 均用 RGBA16F/RGBA8/Depth24Stencil8，无 R32F/RG16F 颜色附件——Defer 不受影响；BRDF LUT（RG16F）从 GL_RGB→GL_RG 修正采样语义。T1 完成需回归后处理 + PBR/IBL App。

### 3.2 T2：深度 cubemap attach

现状：`attachCubeFace`（计划E 新增）只支持 `GL_COLOR_ATTACHMENT0` + `glDrawBuffer(GL_COLOR_ATTACHMENT0)`。PointLightShadow 需要把整个深度 cubemap 分层挂到 `GL_DEPTH_ATTACHMENT`（`glFramebufferTexture`），并 `glDrawBuffer(GL_NONE)`/`glReadBuffer(GL_NONE)`。

**改动 1：`src/rhi/core/IRenderTarget.hpp` 新增**

```cpp
virtual bool attachDepthCube(ITexture3D* cube, int mip = 0) = 0;
```

**改动 2：`src/rhi/gl/GLRenderTarget.hpp/.cpp` 实现**

```cpp
bool GLRenderTarget::attachDepthCube(ITexture3D* cube, int mip) {
    if (!cube || mip < 0 || !_fbo) return false;
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                         static_cast<GLuint>(reinterpret_cast<uintptr_t>(cube->handle())), mip);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    return true;
}
```

分层渲染时 GL 自动写 `gl_Layer`（配合 GS）。绑定语义与 `attachCubeFace` 一致：只负责 attach + draw/read 状态，不管理 FBO 绑定生命周期（挂接前由 App 调 `setRenderTarget`，用毕 `setRenderTarget(nullptr)` 恢复）。

## 4. App 迁移设计

### 4.1 通用模式（沿用计划C/D/E 惯例）

- 几何：`RhiGeometry::Create(shape, useUv, useNormal, useIndex=true)` 建 cube/plane/sphere；quad 用 `RhiGeometry::CreateFromArray`。
- 编译：`renderer()->createShader()` + `renderer()->createPipeline(layout, shader)`；深度测试 `setDepthTest`。
- 渲染顺序严格：`setPipeline → setVertexBuffer(0/1/2) → setIndexBuffer → setUniform → drawIndexed`（计划E T4 Critical 教训：RHI 顶点属性按 per-pipeline VAO 保存）。
- 深度纹理采样：shadow shader 用普通 `sampler2D` 手动比较（无需 GL_TEXTURE_COMPARE_MODE）。
- 静态辅助函数保持 file-static（若跨 TU 不可见再提升，参照计划E T3）。

### 4.2 T3 GLShadowMapApp（最简阴影）

- 深度 FBO：`IRenderTarget::create({w=ShadowMap, h=ShadowMap, attachments=[{Depth, Depth32F, wrap=Repeat}]})`；采样走 `depthTexture2D()`。
- 3 缓冲：plane（RhiGeometry::Create Plane 索引化）、cube 形状 `_vao`（原生用 `shape`=Rect 的尺寸数据，经 RhiGeometry::Create）、quad（CreateFromArray）。
- 2 管线：`_shadowProgram`（ShadowMapping VS+FS）、`_depthProgram`（Depth VS+FS）。原生 `_shadowProgram` 深度 pass 用的是 ShadowMapping.vs（lightSpaceMatrix uniform）——忠实移植这一行为。
- 渲染：`renderScene2FrameBuffer`（viewport=ShadowMap 尺寸、绑深度 FBO、清 depth、画 plane+cube、恢复 viewport/FBO 0）→ `reanderFraemBuffer`（debug 深度可视化到 quad）。无 ImGui 内容。
- 注意：原生 `drawScene` 直接渲染（不走 renderBeforeLoop），迁移后同样直接在 drawScene 内执行。

### 4.3 T4 GLShadowApp（方向光 + PCF + debug）

- 深度纹理：`{Depth, Depth32F, wrapS/T=ClampToBorder, borderColor={1,1,1,1}, Nearest}` → 走 T1 borderColor 扩展。
- 4 管线：`_shadowProgram`（ShadowMapping VS+FS）、`_depthProgram`（ShadowMappingDepth VS+FS）、`_debugProgram`（DebugQuand VS+FS，画深度 debug）。
- 几何：cube、plane、quad（Rect CreateFromArray）。
- `renderScene`：多 model 矩阵（平面 + 3 个缩放立方 + light 立方）+ 每立方 `type` uniform 切换；`renderCube/renderPlane` 严格渲染序。
- ImGui：5 checkbox（Debug/DepthMap/Bias/CullFace/SimplePCF）。
- `renderScene2FrameBuffer`：viewport→ShadowMap 尺寸、绑深度 FBO、清 depth、可 `cullFace(Front)`（`_enableCullFace`）、renderScene、恢复。
- CullFace：RHI `IPipeline` 已有 `setCullFaceEnable(bool)`/`setCullFace(CullFace)`，直接使用即可。

### 4.4 T5 GLPointLightShadowApp（点光源 + 深度 cubemap + GS）

- 深度 cubemap：`ITexture3D::createEmpty({Depth32F, Nearest/Nearest, ClampToEdge×3}, w, h)` 分配 6 面深度存储 → `IRenderTarget` 经 T2 `attachDepthCube` 挂 DEPTH_ATTACHMENT。
- 深度 pass 用 **3-stage 管线**（VS+FS+GS）：`ShaderStage::Geometry` 已有。GS 写 `gl_Layer`（ShadowMappingDepth.gs），uniform `shadowMatrices[6]` 数组。
- cube 几何=`RhiGeometry::Create`。场景：reverse_normals 大立方（scale 10，cull 临时关闭）+ 4 个缩放置物 + light 小立方。
- uniform：`shadowMatrices[6]`、`far_plane`、`lightPos`、`shadows`（空格切换）、`diffuseTexture=0`、`depthMap=1`（`samplerCube` 手动比较）。
- `renderScene2FrameBuffer` 后恢复 viewport + FBO 0 + clear 颜色/深度。运行时 lightPos 沿 z 轴 sin 摆动。
- ImGui：EnablePCF、EnableShadow checkbox + 相机/灯位文本。

### 4.5 T6 GLSSAOApp（全 RHI + noise RhiImage）

- 现状：`m_modelPipeline` 已是 RHI（Model 全 RHI），GBuffer/SSAO/Blur/Light 4 pass 仍 GLProgram + 裸 glad。
- GBuffer：3 附件（RGBA16F 位置/法线、RGBA8 albedo，均 Nearest）+ 深度（原生 RBO→RHI Depth24Stencil8 attachment）。MRT 多附件由 `create(desc)` 已支持（复用计划D Defer 模式）。
- SSAO 红缓冲：`R32F` 单颜色附件（T1 修 ToGLFormat 后 GL_RED 正确）；Blur 同 R32F。
- noise：CPU `GenerateSSAONoise()` 生成 4×4 RGBA32F → `ITexture2D::init(desc, view)`（或 RhiImage 路径）上传，Repeat/Nearest。
- 4 管线全换 RHI：GBuffer/SSAO/SSAOBlur/Light。quad=`CreateFromArray`（原生 interleaved pos3+uv2 → CreateFromArray 交错 pos+uv）。
- renderGBuffer：Model（RHI）+ room cube（invertedNormals）。renderSSAOTexture：绑 gPos0/gNormal1/texNoise2 + 64 kernel uniform + projection。renderBlur：绑 ssaoInput0。renderLight：绑 gPos0/gNormal1/gAlbedo2/ssao3 + view-space light uniform。
- kernel 数组每帧重算为原生行为，忠实保留（可注明非回归）。
- ImGui：EnableSSAO checkbox。

## 5. 风险与对策

1. **深度附件 filter/wrap/border 改动影响既有后处理（Hdr/Bloom/Defer）与 PBR/IBL**：T1 改完即全量回归 + 3 后处理 App + BRDF LUT App 单跑冒烟。
2. **ToGLFormat R32F→GL_RED（SSAO 红缓冲）、RG16F→GL_RG（BRDF LUT）**：已确认既有颜色附件无 R32F/RG16F 冲突（Defer/Hdr/Bloom 均 RGBA16F/RGBA8）；BRDF LUT 采样语义修正。验证同 1；视觉无法自动比对，靠无 GL error + run OK + 评审推理（沿用披露盲区）。
3. **PointLightShadow GS + 深度 cubemap 分层**：验证 `gl_Layer` 写入与 attachDepthCube 状态。
4. **CullFace 需求**：GLShadowApp（_enableCullFace）/GLPointLightShadowApp（reverse_normals 时临时关 cull）需启停背面剔除。RHI `IPipeline::setCullFaceEnable/setCullFace` 已具备，直接使用。
5. **SSAO noise/kernel**：noise initApp 上传一次；kernel 保留每帧重算忠实移植。

## 6. 验证与收尾

- 每任务：build + 全量 46/46 + 单 App 冒烟。
- 提交直接落 develop。
- 每任务更新 SSDD workspace ledger + PROGRESS.md。
- 终审通过后删 `.superpowers/sdd/2026-08-11-rhi-migration-phase-f-shadows-ssao/` workspace。
- 累计迁移 App：27 + 4 = 31；剩余约 15。
- 技术债更新：Phase D 债①（深度附件 filter/wrap）本批兑现修复；新延后项如实记录。