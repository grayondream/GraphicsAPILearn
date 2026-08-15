# VK/GL 运行时热切换 UI 设计

> 背景：本项目为图形 API 学习引擎（renderLearn），支持 GL 与 Vulkan 后端、46 个渲染样例。当前通过命令行 `-b backend -a app` 一次性选定后端与样例，运行时不可切换。
> 目标：把「后端 + 样例」选择改到 UI（ImGui 总控条），支持运行时热切换——在进程中重建渲染上下文与场景，不重启进程。

## 动机与现状

### 现状
- `main.cpp:84-85` 解析 `-b/-a`，`AppFactory.create(gtype, type)` 一次性创建 `IApplication`，运行后无法切换。
- `IApplication` 把「窗口(GLFWWFWindow) + GPU renderer + 样例 RHI 资源」强耦合在同一对象层级：
  - `IApplication` ← `Application`（持有 `m_window`、`m_imguiWindow`、主循环 `run()`）
  - `Application` ← `App`/`GLApp`（持有 `_renderer`、`renderer()`、`aspectRatio()`、`clearColor/begin/endDrawScene`）
  - `GLApp` ← `GLCameraBaseApp`（`_camera` + onKey/onMouse 输入）← 30 个样例继承
  - 其余样例直接或经 `GLPBRBaseApp` 继承 `App`。
- 各 RHI 资源一律为 `shared_ptr`（`_pipeline`/`_vb`/`_texture`/`_uboBuffer`），析构自动释放。
- `GLApp::initGraphics`（GLApp.cpp:23-67）按 `props.vulkan` 分支创建 VK 或 GL renderer + 对应 imgui window（`ImGuiVulkanWindow` 或 `ImGuiOpenglWindow`）。
- `GLFWWindow::initialize`（GLFWWindow.cpp:106-108）创建窗口时按 `props.vulkan` 写死 `GLFW_CLIENT_API`（VK 用 `GLFW_NO_API`），故 GL↔VK 后端切换需重建原生 GLFW 窗口。

### 需求
- UI 提供一个「总控条」（导航窗口）承载后端选择 + 样例选择 + 当前信息。
- 选择后执行运行时热切换，不退出进程。
- 保留各样例自身已有的 ImGui 面板。

## 目标架构：外壳 + 可替换样例

把「常驻运行外壳」与「可替换样例」分离。

### 外壳层（AppHost，由 Application/GLApp 演进，唯一常驻对象）
职责收窄为：
- 持有 `GLFWWindow`、`rhi::IRenderer`、`IImGuiWindow`、当前 `GraphicsType` + `AppType`。
- 主循环 `run()`：`beginFrame → drawScene → imgui 渲染 → endFrame`；每帧间隙检查 pending 切换。
- 把输入事件（onKey/onMouse/onScroll/onButton/onResize）转发给当前 Sample。
- 提供切换入口：`setSample(AppType)` / `setBackend(GraphicsType)`，在帧间隙安全执行重建。
- 渲染总控条 UI。

### 样例层（Sample 接口，可替换）
```cpp
class Sample {
public:
    virtual ~Sample() = default;
    virtual bool load(rhi::IRenderer& renderer) = 0;   // 用 renderer 重建 RHI 资源
    virtual void draw(float dt) = 0;
    virtual void onKeyPress(int, int, int, int) {}
    virtual void onMouseMove(double, double) {}
    virtual void onMouseScroll(double, double) {}
    virtual void onMouseButton(int, int, int) {}
    virtual void onResize(int, int) {}
protected:
    std::shared_ptr<rhi::IRenderer> _renderer{};
    float _aspect{1.0f};
};
```
- RHI 资源用 `shared_ptr`，`load()` 时创建，样例析构自动释放。
- 样例不持有窗口/渲染循环，只暴露 load/draw + 输入钩子。
- `_aspect`（宽高比）由外壳在 init/每帧注入或经 draw 参数提供。

### chunk 决策（用户已确认）
1. **切换方式**：运行时热切换（不重启进程）。样例切换复用窗口 renderer；后端切换重建 GLFW 窗口(同进程)+renderer+imgui。
2. **UI 位置**：独立恒显总控条（backed Combo + 样例下拉 + 信息行），保留各样例自身面板。
3. **迁移策略**：完全重构为 `Sample` 接口（46 个样例类从 `IApplication` 子类改为实现 `Sample`）。

## 后端切换细节

- **重建 GLFW 窗口**：`m_window->shutdown()`（销毁原生窗口）→ `setProperties(vulkan=新值)` → `initialize()` 按新 client API 重建同尺寸窗口（同一进程内，非新 OS 窗口）。
- **重建 renderer**：`_renderer.reset()` → `createGLRenderer()/createVKRenderer()` → `init(新 GLFWSurface)` → `setViewport(...)`。GL 需 `initGLContext`（make current + GLAD）；VK 创建 surface + swapchain。
- **重建 imgui**：`m_imguiWindow->destroy()` → 按新后端创建 `ImGuiOpenglWindow` 或 `ImGuiVulkanWindow`（VK 需 `setRenderer(新 renderer)`）→ `init()`。
- **切换时机**：不能在活跃 frame 内（beginFrame/endFrame 之间）。切换置 `pending` 标记，在 `run()` 的帧间隙（endFrame 之后、下一 beginFrame 之前）执行安全重建。
- **样例 reload**：后端重建后 `current->load(新 renderer)` 重建全部 RHI 资源。

## 执行拆分

| 步骤 | 内容 |
|------|------|
| 1 | 新建 `Sample` 接口与 `BaseSample`（相机交互样例基类，含 `_camera` + 输入处理 + `_aspect`），不接任何样例 |
| 2 | `AppHost`（Application 演进）持有 current Sample；主循环转发输入；`setSample`/`setBackend`（帧间隙重建） |
| 3 | `SampleFactory`（按编译宏过滤后端；按 AppType 建 46 样例） |
| 4 | 逐类迁移 46 个样例：改基类 + `initApp→load`、`drawScene→draw`、`renderer()→_renderer`、`aspectRatio()→_aspect`、输入 override 接入 Sample。逐批编译验证 |
| 5 | 总控条 UI（后端 Combo + 样例下拉 + 信息行）挂到 AppHost |
| 6 | 切换执行路径（帧间隙重建 + reload）接线 |
| 7 | 全量回归（GL 46 + VK 46 + 热切换冒烟） |

## YAGNI 裁剪

- 不做 DX11 后端（不启用则不编译，仅留预留）。
- 不做样例间传参/状态保持（切换直接重置样例）。
- 总控条先做 Combo 选择，不做搜索过滤。
- 后端切换复用现有代码路径（GLApp.cpp 的 if vulkan 分支迁入 AppHost），不重写后端 init 逻辑。

## 验证方法

- 构建：`./scripts/build_run.sh build`（ENABLE_OPENGL=ON + ENABLE_VULKAN=ON）。
- 回归：GL 46 App 冒烟 + VK 46 App 冒烟（各 exit 干净、无崩溃）。
- 热切换冒烟：启动后通过总控条切换样例（同后端）、切换后端（GL↔VK），dumpFrame 验证画面（如 Cube 中央立方体、ImGui 面板显现）与 3D 翻转未破坏。
- 3D 翻转修复判据：SimpleTexture 上下方向（top>bottom、left>right）在切换后仍成立。
