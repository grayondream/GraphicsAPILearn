# VK 端 ImGui 覆盖层设计

日期：2026-08-15

## 背景与目标

项目同时支持 OpenGL 与 Vulkan 两个 RHI 后端，App 层（`src/app/GL/`）共用，并在每帧通过 `IImGuiWindow`（`src/app/IImGuiWindow.hpp`）接口渲染 Dear ImGui 覆盖层。

- **GL 端**：`ImGuiOpenglWindow` 完整可用（`imgui_impl_glfw` + `imgui_impl_opengl3` 双 backend，在 3D 之后、swap 之前用裸 GL 绘制 overlay）。
- **VK 端**：当前 `ImGuiContextWindow` 只初始化 ImGui **核心上下文**、不绑定渲染 backend，因此 Vulkan 画面不含 ImGui 覆盖层（有 context + 每帧 NewFrame，面板代码不崩但不可见）。

目标：让 Vulkan 后端把 ImGui 真正渲染到画面，与 GL 端行为一致，并复用现有 App 面板代码（各 `GL*App` 的 `ImGui::Begin/End`、基础 FPS 窗口）自动生效。

## 现状事实（来自代码勘察）

- ImGui 核心库来自 vcpkg（`imgui::imgui`，版本 1.92.8，含 `imgui.h/imgui_internal.h`）。
- 平台/渲染 backend 源码 vended 在 `3party/imgui/`：根目录有 `imgui_impl_glfw.{h,cpp}`；子目录 `OpenGL/`、`Vulkan/`、`DX11/`、`DX12/`、`Metal/`。
- `3party/imgui/CMakeLists.txt`：`ENABLE_OPENGL` 分支收集 `OpenGL/imgui_impl_opengl3.cpp`；**Vulkan backend 分支被注释删除**，原因（PROGRESS 记录）是 vendored 旧 backend 用 `ImDrawCmd::TextureId`，与 vcpkg 新版 imgui 的 `TexId`/`ImTexture*` API 不兼容，编译失败。
- `ImGuiContextWindow`（`src/app/GL/ImGuiContextWindow.{hpp,cpp}`）：`init` 建 context + Build 字体纹理；`newFrame` 手算 DisplaySize/DeltaTime + `ImGui::NewFrame`；`render` 只 `ImGui::Render()`，DrawData 丢弃。
- `GLApp::initGraphics` vulkan 分支（`GLApp.cpp:26-43`）建 `ImGuiContextWindow`。
- VK 的 ImGui 覆盖层插入点：`Application::render` 的 `m_imguiWindow->render()`（`Application.cpp:152-154`），位于 `drawScene` 之后、`endDrawScene`（含 `VKRenderer::endFrame` 的 `endRenderPass`）之前。

### VK 关键状态（VKBackend.cpp）

- 单 `_cmd` 命令缓冲，`VKRenderer::beginFrame`（415-426）acquire + begin；`endFrame`（428-448）`_rpActive` 则 `endRenderPass` 后 `end` + submit。
- render pass `_renderPass`（263-289）单颜色附件、`samples=e1`、`finalLayout=ePresentSrcKHR`；framebuffer 按 image 数建好在 `_framebuffers`（293-308）；swapchain ≥3 image。
- 3D 坐标翻转修复：`applyViewport`（899-912）负高度 viewport `(x, y+h, w, -h)`；pipeline 创建与动态状态里 front face 反转为 `eClockwise`（`VKPipeline.cpp:181/256`）。
- descriptor：单布局 binding0=UBO、binding1..15=sampler；descriptor pool `_dsPool` maxSets 255；UBO 32 槽 ring（`_uboDs[_uboSlotIndex % 32]`）。
- 私有成员无 public accessor：`_device`、`_graphicsQueue`、`_graphicsFamily`、`_dsPool`、`_cmd`、`_renderPass` 等。
- `VKSwapchain` 暴露 `imageCount()`、`extent()`、`format()`、`currentImage()` 等。

## 设计

### 1. 更新 Vulkan backend（三方）

- 从 Dear ImGui 官方仓库拉取与 vcpkg 1.92.8 匹配的 `imgui_impl_vulkan.{h,cpp}`，替换 `3party/imgui/Vulkan/` 下旧文件（解决 `TextureId` 等 API 不兼容）。
- 在 `3party/imgui/CMakeLists.txt` 重新启用 `ENABLE_VULKAN` 分支，收集 `Vulkan/imgui_impl_vulkan.cpp`（对照现有 `ENABLE_OPENGL` 分支写法）。

### 2. 新增 `ImGuiVulkanWindow`（App 层）

实现 `IImGuiWindow`，替代 `ImGuiContextWindow`：
- `init`：`ImGui::CreateContext()` + `StyleColorsDark()` + `ImGui_ImplGlfw_InitForVulkan(window, ...)`（平台 backend）+ `ImGui_ImplVulkan_Init(&initInfo, renderPass)`（渲染 backend）+ `CreateFontsTexture`（需一次立即提交的命令缓冲）。
- `newFrame`：`ImGui_ImplVulkan_NewFrame()` + `ImGui_ImplGlfw_NewFrame()` + `ImGui::NewFrame()`。
- `render`：`ImGui::Render()`，把 `ImDrawData` 交给 VK 后端在同一活跃 render pass 内绘制。
- `destroy`：`ImGui_ImplVulkan_Shutdown()`、`ImGui_ImplGlfw_Shutdown()`、`ImGui::DestroyContext()`。
- `_initInfo` 需 instance / physical device / device / queue family / queue / descriptor pool / image count / 格式，取自 VKRenderer。

### 3. VKRenderer 提供 hook

给 `VKRenderer`（`VKBackend.cpp`）开放只读 accessor + 一个注入绘制点，避免破坏封装：
- accessor：instance、physical device、device、graphics queue、graphics family、descriptor pool、image count、render pass 句柄（供填充 `ImGui_ImplVulkan_InitInfo` 与绑定 `ImGui_ImplVulkan_Init` 的 render pass）。
- 新增绘制方法（如 `renderImGuiOverlay(ImDrawData*)`）：在 `_recording && _rangeActive`（或首次 draw 后 `_rpActive`）为真时，于 swapchain render pass 内调用 `ImGui_ImplVulkan_RenderDrawData(drawData, *_cmd, pipeline)`；随后恢复 3D 动态状态（负高度 viewport + frontFace 是 3D 专属，ImGui backend 自设正高度 viewport + 无 cull，二者在同一个 `_cmd` 内经动态状态隔离，互不覆盖）。
- 需要时懒建 ImGui 专用 pipeline（匹配 swapchain render pass 与格式、无 cull、禁用 depth test/write）并缓存，参照现有 `pipelineFor` per-render-pass 懒建惯例。

### 4. 接线

- `GLApp::initGraphics` vulkan 分支（当前建 `ImGuiContextWindow`，`GLApp.cpp:40-41`）改为建 `ImGuiVulkanWindow`，把 `VKRenderer` 相关句柄传入其 `init`。
- 挂载点复用 `Application::render` 的 `m_imguiWindow->render()`（153-154 行），天然在 3D 之后、`endRenderPass` 之前，为最上层。

### 5. 验证/测试

- 构建目标：`imgui_impl_vulkan.cpp` 编译通过、链接无缺。
- 运行时：`-b vulkan` 跑 Triangle/Cube/Hdr，ImGui 面板（FPS、Slider/Checkbox）显示在画面且文本不颠倒、可交互。
- 回归：3D 翻转修复不被破坏（dog.jpg 上亮下暗判据）。

## 风险与注意

- 坐标系：ImGui backend 自设正高度 viewport（左上原点），与 3D 负高度 viewport 分离，overlay 不会翻转；ImGui pipeline 不绑 3D UBO descriptor。
- ImGui 需要独立 descriptor pool（backend 用自己的 pool 或用 VKRenderer 的 `_dsPool`），避免与 3D ring set 冲突。
- swapchain render pass `samples=e1`：ImGui pipeline 采样需匹配。
- 范围外/暂不做：swapchain window resize 后重建 ImGui（当前 `VKSwapchain::resize` 从未被触发）；多 viewport/docking；中文/自定义字体。
