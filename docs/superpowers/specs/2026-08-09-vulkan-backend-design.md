# Vulkan 后端与全量 RHI 迁移设计

日期：2026-08-09
状态：已评审（用户已确认全部关键决策）

## 目标

在现有 RHI（`src/rhi/core/` + `src/rhi/gl/`）基础上，将 **46 个 `GLXxxApp` 全部迁移到 RHI 抽象**，实现 **Vulkan 后端**，使所有 App 同时可跑 OpenGL 与 Vulkan，以 `run.sh all -b vulkan` 全量运行并通过真实渲染验证。

## 背景与现状

- 已完成：`rhi/core` 11 个接口头、`rhi/gl` GL 后端、`GLApp` 通过 `IRenderer` 初始化（`_renderer->clearColor/present/setViewport`）。
- **未完成**：46 个 App 内部仍直接调用原生 GL（`glDrawArrays`、`GLProgram`、`GLCube` 等），RHI 接口层仅覆盖基础 `draw/drawIndexed`，无法表达 46 个 App 的高级特性。
- Vulkan 环境就绪：vcpkg 有 `vulkan.hpp`/`shaderc`/`spirv-reflect`/`vulkan-memory-allocator`；系统 `libvulkan.so.1.4.357`；有 `lvp` 等 ICD；`/usr/bin/glslangValidator`（无 `glslc`）；GLFW 支持 `glfwCreateWindowSurface`；`DISPLAY=:0`。
- CMake 已声明 `ENABLE_VULKAN` option（默认 OFF）。

## 46 个 App 的 GL 特性全集（接口设计的输入）

由 4 个并行探索 agent 梳理，按类别汇总：

| 类别 | 覆盖 | 复杂度上限 |
|---|---|---|
| 缓冲/顶点 | VAO/VBO/EBO；交错（pos+color stride 32B）与分离布局（uv/normal 独立 VBO）；最多 3 VBO + EBO | `MeshVertex` 7 属性（含 `glVertexAttribIPointer` 骨骼 int4） |
| 绘制 | `glDrawArrays` / `glDrawElements` / `glDrawElementsInstanced` | Saturn 30000 实例 + mat4 拆 4×`glVertexAttribDivisor`(loc3-6)；实例属性 loc3 vec2 divisor=1 |
| 状态 | depth test/func/mask；stencil test/func/op/mask（TemplateTest 3 pass）；blend func；cull face + frontFace(CW)；polygonMode(line/point/fill)；`GL_MULTISAMPLE` 开关 | TemplateTest 运行时 3 段 stencil 切换 |
| 纹理 | 2D / cubemap / MSAA 2D；浮点格式（RGBA16F/RGB16F/RG16F/RED/RGBA32F）；mipmap + textureLod；采样器 sampler2D/samplerCube；纹理单元 0~5 | ParallaxMap 3 单元、PBRTexture 5 单元、PBRTexture/Defer/SSAO 多 attachment |
| 渲染目标 | 多 color attachment（最多 3 + glDrawBuffers）；深度 RBO（DEPTH_COMPONENT/24/24S8）或**深度纹理**（shadow map 可采样）；cubemap 深度（PointLightShadow `glFramebufferTexture`）；**动态挂接 attachment**（IBL 换 cubemap 面/mip）；MSAA FBO（`glTexImage2DMultisample`/`glRenderbufferStorageMultisample`）+ `glBlitFramebuffer` resolve | Bloom 3 FBO 13 pass；IBL_Specular 49 pass（5 mip × 6 面 prefilter）；Defer `glBlitFramebuffer` 传深度 |
| shader | VS/FS/GS 三阶段；UBO（`glGetUniformBlockIndex`+`glUniformBlockBinding`+`glBindBufferRange`）；GLSL 内建（gl_VertexID/gl_FragCoord/gl_FrontFacing/gl_PointSize/gl_InstanceID） | PointLightShadow GS `layout(triangles) in; triangle_strip max_vertices=18` + `gl_Layer=face` |
| uniform | mat4/mat3/vec4/vec3/vec2/float/int/bool + 结构体点语法（`material.ambient`）+ 数组（`pointLights[4]`、`lights[16/169]`、`samples[64]`、`shadowMatrices[6]`） | Defer `Light lights[169]`（CPU 13×13 网格） |
| 多 pass | 场景→FBO→后处理 quad（`GL_TRIANGLE_STRIP` 全屏 quad）；ping-pong FBO（Bloom）；`renderBeforeLoop()` 预计算（IBL） | 见上表 |
| 相机/ImGui | `GLCameraBaseApp` 提供 WASD/鼠标/滚轮相机；ImGui 控件（checkbox/slider/color/input）控制渲染参数 | 大量 App |

**关键结论**：接口必须覆盖 geometry shader、instancing（含实例属性/矩阵拆分）、多 MRT（3 attachment）、深度纹理（可采样）、cubemap attachment（含动态挂接与 mip）、MSAA resolve、UBO（range 绑定）、浮点纹理、全屏 quad、运行时 stencil/blend/cull 状态切换。

## 设计决策（用户已确认）

| 决策点 | 选择 |
|--------|------|
| 架构路径 | 先完成 RHI 迁移，再写 Vulkan 后端 |
| 迁移范围 | 全部 46 个 App |
| 接口设计 | 一次设计完整接口集（覆盖全部 GL 特性），分阶段实现验证 |
| 状态模型 | 命令式状态接口 + Vulkan dynamic state（Vulkan 1.3+ `VK_EXT_extended_dynamic_state`） |
| uniform 抽象 | **统一显式 UBO 接口**（`createUniformBuffer` + `bindBufferRange`），App 自管布局；shader 双版本均用 uniform block（std140）。`setUniform` 仅作过渡便捷层 |
| 技术栈 | vcpkg `vulkan.hpp` + 构建期 glslangValidator 编译 SPIR-V |
| GL 后端 | 保留，作为 Vulkan 回归对照 |
| 验收标准 | 真实渲染验证：每个 App Vulkan 运行 + 截图对比 GL |
| App 命名 | 迁移后 `GLXxxApp` → `XxxApp`（去 GL 前缀），基类 `GLApp`→`App`、`GLCameraBaseApp`→`CameraBaseApp`；`GLAppFactory` 改注册表 |
| shader 组织 | `res/Vulkan/` 镜像 `res/GL/` 目录结构，构建期编译 SPIR-V |
| 文档化 | 写 spec + 分阶段实施计划 |

## 总体架构

```
src/
├── rhi/
│   ├── core/            # 纯接口层（本次大幅扩展，无 GL/VK 依赖）
│   ├── gl/              # GL 后端（保留，适配新接口）
│   └── vk/              # Vulkan 后端（新增）
├── app/
│   ├── Application.hpp  # 窗口/渲染循环（已走 IRenderer）
│   ├── App.hpp          # GLApp 去 GL 化改名
│   ├── CameraBaseApp.hpp# GLCameraBaseApp 去 GL 化改名
│   ├── Base/Light/Advanced/Model/  # 46 个 XxxApp（RHI 化，去 GL 前缀）
│   └── AppFactory        # 注册表
├── geometry/            # 保留（toGL/toDX11 之外加 toVulkan 或去除 API 概念）
└── model/               # Model/Mesh 去 GL（GLProgram→IShader/IPipeline）
```

`res/Vulkan/` 镜像 `res/GL/`：每个 App 的 shader 目录存 `*.vert/*.frag/*.geom`（#version 450，`layout(set=0, binding=N)`），构建期 `glslangValidator -V` 编译为 `.spv` 于 `build/res/Vulkan/...`。

## 四阶段分解

| 阶段 | 内容 | 产出 | 验证 |
|---|---|---|---|
| **A** | RHI 接口层完整设计 + 扩展（core 接口 + GL 后端适配新接口） | 完整接口集，46 App 所有特性可表达 | 编译通过；现有 GL App 回归不变 |
| **B** | 46 App 分批迁移到 RHI（去 GL 前缀，AppFactory 注册表） | 46 个 `XxxApp` + `App`/`CameraBaseApp` | 每批 `run.sh all -b gl` 全 OK，与迁移前渲染一致 |
| **C** | Vulkan 后端（`rhi/vk`：instance/device/swapchain/pipeline/descriptor/FBO/blit/UBO/dynamic state） | 完整 VK 后端 | 基础 App（Triangle→Camera→Light 等）Vulkan 跑通 |
| **D** | `res/Vulkan` shader 编译集成 + 全量验证 | SPIR-V 构建流水线 + `run.sh all -b vulkan` | 46 App Vulkan 全部运行 + 截图对比 GL |

## 接口层设计（子项目 A 的核心产出）

### 1. `Common.hpp` 扩展

- `ShaderStage`：新增 `Compute` 类型 + `entry` 字段（默认 "main"）+ `sourceIsSPIRV` 标志（GL 后端读 GLSL 文本，Vulkan 后端读 .spv）。
- `VertexElement`：新增 `binding`（GL 分离 VBO → Vulkan 多 vertex buffer binding）、`inputRate`（`VertexInputRate::PerVertex/PerInstance`）、`format` 扩展 `Float2/3/4, Int4`（骨骼 id 用）。
- 新增 `TextureFormat` 枚举：`RGB8, RGBA8, RGBA16F, RGB16F, RG16F, R32F, Depth32F, Depth24Stencil8`。
- 新增 `FramebufferAttachment`：`{ AttachmentType type; TextureFormat format; bool external; samples }`（color/depth/stencil，external=true 时由 App 提供纹理句柄）。
- 新增 `FramebufferDesc`：`{ int width, height; int samples; std::vector<FramebufferAttachment> attachments }`。
- 新增 `StencilState`（func/opSfail/opDpfail/opDppass/reference/mask）、`BlendState`（src/dst 因子）。

### 2. `IBuffer` 扩展

- `BufferType` 加 `Uniform`。
- `update(const void* data, size_t size, size_t offset)` → GL `glBufferSubData` / Vulkan `vkCmdCopyBuffer`。
- `bindRange(uint32_t binding, size_t offset, size_t size)` → UBO 绑定（GL `glBindBufferRange`；Vulkan 写 descriptor set）。

### 3. `ITexture2D/ITexture3D` 扩展

- `init(const TextureDataView&)` 扩展为可指定 `TextureFormat`、wrap/filter、mipmap。
- `createEmpty(int w, int h, TextureFormat, bool msaa, int samples)`：无数据分配（渲染目标/深度纹理用）。
- cubemap：`ITexture3D` 支持 6 面加载（`initCube(const TextureDataView& faces[6])`）。
- 新增 `depthTexture()` 语义：深度纹理可采样（shadow map）。
- 采样器在 Vulkan 中为独立对象，`ITexture2D` 内部持有（后端具体实现），App 无感。

### 4. `IShader` 扩展

- `compile(stages)` 支持 VS/FS/GS/CS 四阶段（现有已含 Geometry）。
- 源为文件路径（GLSL 或 SPIR-V），后端按 `sourceIsSPIRV` 解释。
- 保留 `getLog()/valid()`。

### 5. `IPipeline` 状态命令（命令式全集）

```cpp
void setDepthTest(bool);
void setDepthFunc(CompareFunc);       // Less/LessEqual/Always/Never...
void setDepthMask(bool);              // 写深度开关
void setStencilTest(bool);
void setStencilFunc(StencilFunc, int ref, unsigned mask);
void setStencilOp(StencilOp sfail, StencilOp dpfail, StencilOp dppass);
void setStencilMask(unsigned mask);
void setBlend(bool);
void setBlendFunc(BlendFactor src, BlendFactor dst);
void setCullFace(bool enable, CullFace face);   // Back/Front
void setFrontFace(bool ccw);                     // GL_CW↔ccw=false
void setPolygonMode(PolygonMode);                // Fill/Line/Point
void setMultisample(bool);
bool setUniform(const std::string& name, ...);   // 便捷层（GL 直连，Vulkan 可后续忽略或映射）
void bindUniformBlock(uint32_t binding);         // 显式 UBO 声明
```

Vulkan 后端：这些命令在 pipeline 创建后记录；静态的（initApp 期设置）固化进 `VkPipeline`；`drawScene` 期动态切换的（TemplateTest stencil、Shadow cull、Msaa multisample）走 **dynamic state**（`VkDynamicState`：depth test/write/compare、stencil、blend、cull、frontFace、polygonMode、viewport/scissor）。

### 6. `IRenderTarget`（Framebuffer 抽象）

```cpp
bool create(const FramebufferDesc& desc);       // 多 attachment + depth + samples
void attachCubeFace(ITexture3D* cube, int face); // IBL 动态挂接
bool bind();  bool unbind();
ITexture2D* colorTexture(int i = 0);            // 供采样/后处理（null 表示无）
ITexture3D* depthTexture();                      // 深度纹理（shadow map 可采样，cubemap 时返回 ITexture3D*）
bool resolveTo(IRenderTarget& dst);              // MSAA blit resolve
void* handle();
void release();
```

### 7. `IRenderer` 扩展

```cpp
// 生命周期/工厂（保留）
createShader/createPipeline(layout, shader)/createBuffer/createTexture2D/createTexture3D/createRenderTarget
createUniformBuffer();                            // 显式 UBO 工厂
getSwapchain();
// 帧控制（保留）
beginFrame/endFrame/present
// 状态与绘制（扩展）
clearColor/setViewport/setPipeline
setVertexBuffer(buffer)/setVertexBuffer(buffer, binding, stride)   // 多 binding（分离 VBO）
setIndexBuffer(buffer)
setRenderTarget(rt);                              // null=默认 framebuffer
bindTexture(texture, unit)
draw(vertexCount, firstVertex)
drawIndexed(indexCount, indexOffset, vertexOffset)
drawIndexedInstanced(indexCount, instanceCount)
drawInstanced(vertexCount, instanceCount)
blitFramebuffer(srcRT, dstRT, w, h);              // GL blit / VK resolve+blit
backendCapabilities();                            // 查询 MSAA max、UBO max size
```

### 8. `VertexLayout` 完整化

现有 `VertexLayout.elements` 已含 offset/stride/semantic；本次加 `binding` 与 `inputRate`。多 VBO 时多个 `VertexLayout`（每 buffer 一个）或一个 layout 多 binding。Vulkan 后端按 binding 生成 `VkVertexInputBindingDescription[]` + `VkVertexInputAttributeDescription[]`。

### 9. GL 后端适配要点（子项目 A 内同步完成）

- `GLShader`：支持四阶段 + SPIR-V 标志（GL 不加载 SPIR-V，仅解释 GLSL；若 sourceIsSPIRV 置为 true 则 GL 后端报错并提示）。
- `GLPipeline`：新增全部状态命令（glEnable/glDepthFunc/glStencilFunc/glBlendFunc/glCullFace/glFrontFace/glPolygonMode/glEnable(GL_MULTISAMPLE)）；`bindUniformBlock` 实现 `glGetUniformBlockIndex+glUniformBlockBinding`。
- `GLBuffer`：`Uniform` 类型 + `update`（glBufferSubData）+ `bindRange`。
- `GLTexture2D/3D`：浮点格式、createEmpty、cubemap 6 面、MSAA 纹理。
- `GLRenderTarget`：多 attachment、深度纹理、cubemap 挂接、`glBlitFramebuffer` resolve。
- `GLBackend`：新增 `createUniformBuffer`、`setRenderTarget`、`drawInstanced/drawIndexedInstanced`、`blitFramebuffer`、`backendCapabilities`。

## App 迁移模式（子项目 B）

固定模式，每个 App 一批：

1. `GLXxxApp` → `XxxApp`（去前缀），基类引用改为 `App`/`CameraBaseApp`。
2. 资源创建：`GLProgram::init` → `renderer()->createShader()` + `createPipeline(layout, shader)`；`GLCube/GLSphere/GLPlane::init` → `renderer()->createBuffer()` + 显式 `VertexLayout`（多 binding 表达分离 VBO）；`GLImageTexture2D` → `renderer()->createTexture2D()` + `init`（保留 stb 加载于 base/Image）。
3. 绘制：`glBindVertexArray(vao)+glDrawArrays/glDrawElements` → `renderer()->setPipeline(p)+setVertexBuffer(...)+setVertexBuffer(uv, binding=1)+...` + `renderer()->draw/drawIndexed/drawIndexedInstanced`。
4. uniform：**统一走显式 UBO**（`createUniformBuffer` + 结构体填充 + `bindBufferRange`）。GL 版 shader 与 Vulkan 版 shader 都改写为 uniform block 风格（`layout(binding=0) uniform Matrices { ... };`，std140），两版语法高度兼容（GL `#version 430` vs Vulkan `#version 450` + `layout(set=0,binding=N)`），App 的 uniform 代码两端通用。`setUniform` 便捷层仅保留给未迁移/极简 App 过渡，不作为主路径。这样 App 内不再出现"按后端分支填 uniform"的代码。
5. 状态：initApp 内的 glEnable 调用 → pipeline 状态命令（或 App::initGraphics 统一开启，个别 App 覆写）；drawScene 内动态切换 → 直接调 pipeline 状态命令。
6. 多 pass：`renderer()->setRenderTarget(fbo)` 绑定 → draw → `setRenderTarget(nullptr)`；MSAA resolve 用 `blitFramebuffer`。
7. 每批编译 + `run.sh all -b gl -a <app>` 运行验证与迁移前一致。

## Vulkan 后端实现（子项目 C）

`src/rhi/vk/`：

| 文件 | 职责 |
|---|---|
| `VKHeader.hpp` | `vulkan/vulkan.hpp` + `VK_USE_PLATFORM_XLIB_KHR`（或随 GLFW） |
| `VKBackend.hpp/.cpp` | instance/device/queue、GLFW surface、swapchain、command pool、begin/end frame、present |
| `VKSurface.hpp` | `ISurface` 实现，包装 `VkSurfaceKHR` |
| `VKSwapchain.hpp/.cpp` | swapchain 管理（acquire/render/present、resize） |
| `VKShader.hpp/.cpp` | 加载 `.spv` → `VkShaderModule` |
| `VKPipeline.hpp/.cpp` | `VkPipeline`（含 dynamic state）+ descriptor set layout + `setUniform`（显式 UBO 写入）+ 状态命令 |
| `VKBuffer.hpp/.cpp` | vertex/index/uniform（host-visible）缓冲 |
| `VKTexture2D/3D.hpp/.cpp` | 纹理 + sampler + 布局转换 barrier |
| `VKRenderTarget.hpp/.cpp` | `VkRenderPass` + `VkFramebuffer` + 多 attachment + resolve |
| `VKImageTexture2D/3D` | stb 加载（复用 base/Image） |

**Shader 编译**：构建期 CMake 自定义命令：`res/Vulkan/**/*.{vert,frag,geom}` → `glslangValidator -V --target-env vulkan1.3 -o build/res/Vulkan/**/*.spv`。`StaticCollector` 增加 `getVulkanShaderPath()`，App 通过 `renderer()` 判断后端选路径。

**状态映射**：
- pipeline 创建时用默认状态；所有可动态状态注册为 `VkDynamicState`。
- 绘制前按 RHI 状态命令 `vkCmdSetDepthTestEnable/...` 设置。
- 该方案要求 device 支持 `VK_EXT_extended_dynamic_state`（Vulkan 1.3 core，lvp 支持）。

**资源生命周期**：command buffer 每帧重建（reset pool）；UBO 用 host-visible + coherent memory，每帧更新；纹理上传用 staging buffer。

## 验证与测试（子项目 D）

- 每阶段 `cmake -S . -B build -DENABLE_OPENGL=ON -DENABLE_VULKAN=ON` + 全量编译。
- 阶段 B 每批：`run.sh all -b gl` 全量 OK 即通过（与迁移前一致）。
- 阶段 C 每新增 Vulkan 能力：对应 App `run.sh all -b vulkan -a <app>`。
- 阶段 D 全量：`run.sh all -b vulkan`（lvp 软件渲染，DISPLAY=:0）+ 截图对比 GL。
- ImGui 在 Vulkan 下用 imgui 自带 vulkan backend 接入。

## 范围外

- compute shader（46 App 均未用 `glDispatchCompute`）——接口预留 `ShaderStage::Compute`，后端不强制实现。
- DX11/DX12/Metal/WebGPU 后端。
- RenderGraph / 统一场景抽象。

## 风险与对策

| 风险 | 对策 |
|---|---|
| Vulkan 后端工作量巨大 | 四阶段分解，每阶段可独立验证；GL 后端为对照 |
| 命令式状态 × Vulkan dynamic state 兼容性 | 仅 initApp 期静态状态固化 pipeline；动态切换全部走 dynamic state；若某 device 不支持则报错 |
| 显式 UBO 与便捷 setUniform 混用 | 明确边界：简单 App 便捷层，大规模 uniform App 显式 UBO |
| IBL 动态挂接 attachment 在 Vulkan 不便 | `IRenderTarget::attachCubeFace` + VK 实现（每面一个 VkFramebuffer 或改 render pass 视图） |
| lvp 软件渲染性能 | 仅验证用；文档注明真实 GPU 更佳 |
| shader 双版本维护 | `res/Vulkan/` 镜像目录隔离；GLSL 430（GL）与 450（VK）语法差异小 |
