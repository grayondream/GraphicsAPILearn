# RHI 架构重构设计

日期：2026-08-08
状态：已评审（第 1 阶段实现目标：Linux + OpenGL）

## 目标

将当前与 OpenGL 强耦合、每个特性一个 `GLXxxApp` 的架构，重构为统一的 **Render Hardware Interface (RHI) 抽象层**，为后续支持多平台（Windows / macOS / Linux）与多图形 API（OpenGL / Vulkan / Metal / DX11 / DX12 / DX9）消除重复代码。

首要目标为**学习驱动**：清晰展示各 API 差异，不追求极致性能。

## 背景与问题

当前架构（仅 Windows + OpenGL 完整实现，约 50 个 GL App）存在以下冗余：

1. **特性×API 爆炸**：每个特性一个 `GLXxxApp`，`drawScene()` 内直接写 `glDrawArrays` 等原始调用，场景逻辑与 API 完全耦合。支持 DX11 需再写 `DX11XxxApp`，6 API × 50 特性 = 300 类。
2. **Native 抽象层太薄**：仅 `ITexture2D/3D` 有跨 API 接口。顶点缓冲、着色器、渲染目标、深度缓冲、管线状态、framebuffer、swapchain 均无抽象，App 只能调原始 API。
3. **Application 与 GLFW 耦合**：`Application::init(GLFWWindowProperties)`、`m_window->beginFrame()` 均为 GLFW 概念。DX11 的 `DX11App::init(HINSTANCE, WindowDesc)` 甚至不符合 `IApplication` 接口。
4. **Native 层零散**：GL 有 `GLCube/GLPlane/GLSphere/GLProgram`，DX11 无对应物。
5. **Factory/注册散乱**：`GLAppFactory` 手写 50 行 switch，`AppType` 枚举 + switch 硬编码。

**核心结论**：当前抽象层级错误——把"场景/特性"与"渲染 API"绑定在同一类，且 Native 抽象不完整。

## 设计决策

| 决策点 | 选择 | 理由 |
|--------|------|------|
| 驱动 | 学习驱动 | 清晰展示 API 差异优先 |
| 第一阶段 | Linux + OpenGL，vcpkg 管理依赖 | 在本地跑通验证架构 |
| RHI 范围 | 最小集合 + 增量演进 | 只抽象当前用到的资源，降低风险 |
| App 处理 | 全量迁移 50 个 GL App | 完整验证 RHI 接口 |
| 窗口 | 统一 GLFW + `ISurface` 抽象原生句柄 | GLFW 跨平台复用，改动最小 |
| Shader | 各后端自管编译，RHI 只暴露 `IPipeline` | 最能展示各 API 差异 |
| 后端架构 | 方案 1：统一接口 + 单后端实现 | 每后端一个独立目录，迁移模式统一 |

## 架构

```
src/rhi/
├── core/                    # 纯接口层（无 GL 依赖）
│   ├── IBuffer.hpp          # 顶点/索引缓冲
│   ├── ITexture2D.hpp       # 纹理（自 native/ 迁移）
│   ├── ITexture3D.hpp
│   ├── IShader.hpp          # 着色器源描述 + 编译
│   ├── IPipeline.hpp        # 顶点布局 + shader + 渲染状态
│   ├── IRenderTarget.hpp    # 渲染目标/FBO/深度缓冲
│   ├── ISwapchain.hpp       # 交换链/表面
│   ├── ISurface.hpp         # 原生平台句柄抽象
│   └── IRenderer.hpp        # 命令入口
├── gl/                      # GL 后端实现（所有 GL 调用限定于此）
│   ├── GLRenderer.hpp/.cpp
│   ├── GLBuffer.hpp/.cpp
│   ├── GLShader.hpp/.cpp
│   ├── GLPipeline.hpp/.cpp
│   ├── GLTexture2D.hpp/.cpp
│   ├── GLImageTexture2D/3D
│   ├── GLRenderTarget.hpp/.cpp
│   └── GLBackend.hpp        # 按 ENABLE_OPENGL 条件编译入口
└── factory.hpp              # 按 GraphicsType 创建后端
```

### 接口职责

| 接口 | 职责 | 覆盖现有代码 |
|------|------|-------------|
| `IRenderer` | `clear`/`setPipeline`/`setUniform`/`draw`/`viewport`/`bindTexture`/`present` | `GLApp` 绘制逻辑 + `glDrawArrays` |
| `IBuffer` | 顶点/索引缓冲（含 vao/vbo/ebo 封装） | `GLCube::vao_/vbos_/ebo_`、各 App 的 `createVertexBuffer()` |
| `ITexture2D/3D` | 纹理 | 现有 `GLTexture2D`/`GLImageTexture2D` |
| `IShader` | 着色器源描述 + 编译（含 `getLog()`） | `GLProgram` 编译部分 |
| `IPipeline` | 顶点布局 + shader + 渲染状态封装 | `GLProgram` 绑定部分 + 各 App 状态调用 |
| `IRenderTarget` | 渲染目标/FBO/深度缓冲 | `GLFrameBufferApp`、`GLSkyboxApp` 等 |
| `ISwapchain` | 交换链/表面 | `m_window->swapBuffers()` |
| `ISurface` | 返回原生平台句柄 | GLFWwindow / HWND / VkSurfaceKHR 来源 |

### 接口设计原则

- 每个接口**最小化**，只声明 50 个 App 实际用到的操作（YAGNI）。
- 后端差异通过**接口方法显式暴露**（如 `IRenderer::setCullMode`/`setDepthTest`），而非藏在 `draw` 内，便于学习时直观对比各 API 语义差异。
- uniform 用 `IRenderer::setUniform(name, value)` 统一，GL 后端内部映射到 `glUniform*`。
- `IShader`（编译）与 `IPipeline`（状态/绑定）**分开**。

## 窗口与数据流

- 窗口统一由 GLFW 管理（跨平台 Win32/Cocoa/X11），通过 `GLFWSurface` 适配器暴露给 RHI。
- `ISurface::nativeHandle()` 返回原生句柄（GL 返回 `GLFWwindow*`；后续 Vulkan 返回 surface 创建参数、DX11 返回 `HWND`）。
- `Application` 不再直接调 `m_window->swapBuffers()`，改为经 `IRenderer::present()`。
- `GLApp::initGraphics()` 中的 `glViewport`、`glEnable(GL_DEPTH_TEST)`、`glClearColor` 收敛到 `IRenderer::initContext(ISurface&)` + 状态设置。

每帧数据流：
```
Application::run()
  → IRenderer::beginFrame()
  → App.drawScene(dt)  →  IRenderer::setPipeline/setUniform/draw/bindTexture
  → IRenderer::endFrame()  →  present()
```

## App 层迁移

改造模式：每个 App 从"持有 GL 对象 + 调 gl*"改为"持有 `IRenderer` 引用 + 调接口"。

以 `GLTriangleApp` 为例：
```cpp
// 迁移后（面向 RHI）
bool GLTriangleApp::initApp() {
    _shader   = rhi->createShader("Base/triangle.vert", "Base/triangle.frag");
    _pipeline = rhi->createPipeline(_shader, /*顶点布局*/);
    _buffer   = rhi->createVertexBuffer(Triangle::data(), /*layout*/);
    return true;
}
void GLTriangleApp::drawScene(float dt) {
    rhi->setPipeline(_pipeline);
    rhi->setVertexBuffer(_buffer);
    rhi->draw(3);
}
```

- 统一命名：`GLXxxApp` → `XxxApp`（去 GL 前缀，已 API 无关）。
- `GLAppFactory` → `AppFactory`，用注册表/宏替代 50 行 switch。
- 迁移分批，每批可编译验证：Base → Triangle/Rect → 纹理 → 相机/几何体 → 光照 → Advanced → PBR。

## CMake

- `rhi/core`（无条件编译）+ `rhi/gl`（`ENABLE_OPENGL` 时编译），App 层链接 `rhi`。
- 后续每新增后端（`rhi/dx11/`、`rhi/vulkan/`、`rhi/metal/`）按各自 `ENABLE_*` 条件编译。

## 错误处理

- 沿用 `ErrorHandle::ExitIfFailed` / `ErrorHandle` 模式。
- 资源创建失败返回 `bool` 或抛错，App 用现有宏处理。
- `IShader` 编译失败提供 `getLog()` 返回后端日志，便于排查 GLSL/HLSL 问题。

## 测试与验证

- 每帧 FPS 显示（ImGui 窗口）作为基本 sanity check。
- 每迁移一个 App，运行确认渲染结果与迁移前一致（截图人工对比）。
- 后续每个新后端用**同一批 RHI App** 跑，天然成为跨 API 一致性测试——本架构最大收益。

## 清理

- `native/` 目录迁移后逐步清空或改名为资源无关层。
- 删除 `GLCube/GLPlane/GLSphere` 等零散几何类（并入 `IBuffer`）。
- `TODO.md` 更新 OpenGL 完成状态。

## 依赖

vcpkg 已装：spdlog、fmt、zlib/zstd、llvm/clang。
待安装：`glfw3`、`glm`、`assimp`；系统包 `libgl-dev libglx-dev libegl-dev`（OpenGL 开发库）。
内置 `3party/`：glad、stbimage、imgui。

## 范围外（后续阶段）

- Vulkan / Metal / DX11 / DX12 / DX9 后端实现。
- RenderGraph / 统一场景等更高级抽象（工程化优先时再做）。
