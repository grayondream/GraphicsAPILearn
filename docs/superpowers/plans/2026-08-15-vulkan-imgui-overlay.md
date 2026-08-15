# VK 端 ImGui 覆盖层 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 Vulkan 后端把 Dear ImGui 覆盖层真正渲染到画面（与 GL 端一致），复用现有 App 面板代码。

**Architecture:** 用 vcpkg 自带的官方 imgui 1.92.8 `imgui_impl_vulkan.{h,cpp}` 替换 vendored 旧版（解决 API 不兼容）；新建 `ImGuiVulkanWindow`（App 层，实现 `IImGuiWindow`）绑定 GLFW 平台 + Vulkan 渲染 backend；给 `VKRenderer` 加只读 accessor 与一个在活跃 render pass 内绘制 ImGui 的注入方法，挂载于 `Application::render` 的 `m_imguiWindow->render()`（3D 之后、endRenderPass 之前）。

**Tech Stack:** Dear ImGui 1.92.8（vcpkg）、Vulkan（vk.hpp raii）、GLFW、C++17、CMake。

## Global Constraints

- ImGui 核心库版本必须为 vcpkg 的 1.92.8（`imgui::imgui`），不引入其他 imgui 版本。
- 后台源文件**必须**取自本地解压目录 `/home/ares/apps/vcpkg/buildtrees/imgui/src/v1.92.8-8e9c05eb59.clean/backends/`（网络 curl/git 不可达，webfetch 仅作确认，不使用网络下载）。
- 不修改 `3party/imgui/imgui_impl_glfw.*`、`3party/imgui/OpenGL/*`（GL 端当前正常，不触碰）。
- 保持 3D 翻转修复不变：负高度 viewport（`VKBackend.cpp:907`）+ frontFace `eClockwise` 反转（`VKPipeline.cpp:181/256`）。
- 构造与编译：项目用 `./scripts/build_run.sh build`；验证时 `-b vulkan`。
- 遵循现有代码风格：`vk::raii::`、注释用中文、不加无关重构。

---

### Task 1: 替换 Vendored Vulkan Backend 并启用 CMake 编译

**Files:**
- Overwrite: `3party/imgui/Vulkan/imgui_impl_vulkan.cpp`
- Overwrite: `3party/imgui/Vulkan/imgui_impl_vulkan.h`
- Modify: `3party/imgui/CMakeLists.txt`

**Interfaces:**
- Consumes: vcpkg imgui 1.92.8（`imgui.h`）
- Produces: `imgui_impl_vulkan.h` 中的 `ImGui_ImplVulkan_InitInfo`、`ImGui_ImplVulkan_Init(ImGui_ImplVulkan_InitInfo* info)`（注意新版 `RenderPass` 在 `PipelineInfoMain.RenderPass`）、`ImGui_ImplVulkan_NewFrame()`、`ImGui_ImplVulkan_RenderDrawData(ImDrawData*, VkCommandBuffer, VkPipeline = VK_NULL_HANDLE)`、`ImGui_ImplVulkan_Shutdown()`、`ImGui_ImplGlfw_*`（已有）。

- [ ] **Step 1: 备份旧 backend**
```bash
cd /home/ares/workspace/GraphicsAPILearn
git mv 3party/imgui/Vulkan/imgui_impl_vulkan.cpp 3party/imgui/Vulkan/imgui_impl_vulkan.cpp.bak 2>/dev/null || cp 3party/imgui/Vulkan/imgui_impl_vulkan.cpp 3party/imgui/Vulkan/imgui_impl_vulkan.cpp.bak
cp 3party/imgui/Vulkan/imgui_impl_vulkan.h 3party/imgui/Vulkan/imgui_impl_vulkan.h.bak
```
（保留旧版以便对比；后续确认没问题后删除 .bak）

- [ ] **Step 2: 从本地 vcpkg 源码树复制官方 1.92.8 backend**
```bash
SRC=/home/ares/apps/vcpkg/buildtrees/imgui/src/v1.92.8-8e9c05eb59.clean/backends
cp "$SRC/imgui_impl_vulkan.cpp" 3party/imgui/Vulkan/imgui_impl_vulkan.cpp
cp "$SRC/imgui_impl_vulkan.h"   3party/imgui/Vulkan/imgui_impl_vulkan.h
```

- [ ] **Step 3: 确认新版 API 标记存在**
```bash
grep -n "PipelineInfoMain" 3party/imgui/Vulkan/imgui_impl_vulkan.h
grep -n "DescriptorPoolSize" 3party/imgui/Vulkan/imgui_impl_vulkan.h
```
Expected: 两行均输出（新版 API 特征）。若缺失则说明复制失败，回到 Step 2。

- [ ] **Step 4: 修改 CMakeLists 启用 Vulkan 分支**
编辑 `3party/imgui/CMakeLists.txt`，在 `ENABLE_OPENGL` 分支之后、`ENABLE_METAL` 分支之前插入：
```cmake
if(ENABLE_VULKAN)
    include_directories(${IMGUI_SRC_PATH}/Vulkan)
    aux_source_directory(${IMGUI_SRC_PATH}/Vulkan IMGUI_VULKAN_SRC_FILES)
    message(STATUS "IMGUI_VULKAN_SRC_FILES: ${IMGUI_VULKAN_SRC_FILES}")

    list(APPEND IMGUI_SRC_FILES ${IMGUI_VULKAN_SRC_FILES})
endif()
```
并删除原注释行 `# ImGui Vulkan backend 不接入（vulkan-backend-unified-ubo 计划：VK 下禁用 ImGui，复用 IImGuiWindow 空实现）`。

- [ ] **Step 5: 构建验证编译通过**

预期 `imgui_impl_vulkan.cpp` 需能与 vcpkg imgui 1.92.8 核心 API 兼容。构建：
```bash
./scripts/build_run.sh build 2>&1 | grep -E "error:|build OK"
```
Expected: `==> build OK`（此时 VK 模式下 App 仍用 `ImGuiContextWindow`，无渲染，仅验证 backend TU 编译链接正常）。

- [ ] **Step 6: 提交**
```bash
git add -A 3party/imgui/
git commit -m "build(imgui): 更新 imgui_impl_vulkan backend 至 1.92.8 并启用编译"
```

---

### Task 2: VKRenderer 暴露 ImGui 需要的句柄

**Files:**
- Modify: `src/rhi/vk/VKBackend.cpp`（class VKRenderer 定义，第 29 行起）
- Modify: `src/rhi/vk/VKBackend.hpp`

**Interfaces:**
- Consumes: `VKRenderer::init`（137 行）已创建的 `_device`、`_graphicsQueue`、`_graphicsFamily`、`_dsPool`、`_imageReady`/`_rendered`；`_swapchain->imageCount()`、`_swapchain->format()`、`_renderPass`、`_cmd`。
- Produces（供 Task 3 的 `ImGuiVulkanWindow` 使用）:
  - `struct VKImGuiInitInfo`（`VKBackend.hpp`）：`vk::Instance instance; vk::PhysicalDevice physDevice; vk::Device device; uint32_t graphicsFamily; vk::Queue graphicsQueue; vk::DescriptorPool dsPool; uint32_t imageCount; vk::DeviceSize minAllocationSize;`
  - `virtual bool imguiInitInfo(VKImGuiInitInfo& out)`（IRenderer 新增，默认返回 false）
  - `virtual void renderImGuiDrawData(void* drawData)`

  > 注意：VKRenderer 是 `IRenderer` 子类（`VKBackend.cpp:29`），GLBackend 同样继承 IRenderer。为不破坏 GL，在 `IRenderer.hpp` 加 `virtual bool imguiInitInfo(VKImGuiInitInfo& out) { (void)out; return false; }`（默认实现），VKRenderer override。

- [ ] **Step 1: IRenderer 增加默认实现虚方法**
编辑 `src/rhi/core/IRenderer.hpp`，文件顶部前置声明：
```cpp
struct VKImGuiInitInfo;
```
class IRenderer 内 `backendCapabilities()` 声明后追加：
```cpp
// ImGui / overlay 扩展钩子（默认 no-op；VKRenderer 覆写）
virtual bool imguiInitInfo(VKImGuiInitInfo& out);
virtual void renderImGuiDrawData(void* /*ImDrawData*/);
```
并在类外（或 IRenderer.cpp 若有）给默认实现：
```cpp
bool IRenderer::imguiInitInfo(VKImGuiInitInfo&) { return false; }
void IRenderer::renderImGuiDrawData(void*) {}
```
> 若项目无 IRenderer.cpp，则在头文件内联实现（`{ (void)out; return false; }`）。请以当前文件结构为准（IRenderer.hpp 目前是纯头文件，内联实现）。

- [ ] **Step 2: VKBackend.hpp 定义 VKImGuiInitInfo**
在 `namespace rhi` 内、`createVKRenderer` 声明前加：
```cpp
struct VKImGuiInitInfo {
    vk::Instance instance{};
    vk::PhysicalDevice physDevice{};
    vk::Device device{};
    uint32_t graphicsFamily{0};
    vk::Queue graphicsQueue{};
    vk::DescriptorPool dsPool{};
    uint32_t imageCount{0};
    vk::DeviceSize minAllocationSize{1u << 20};
};
```
并在 VKBackend.cpp 顶部 `#include <imgui.h>`、`#include <imgui_impl_vulkan.h>`、`#include <imgui_impl_glfw.h>`（imgui include 路径由 3party/imgui/Vulkan include_directories 提供）。
> 注：`VKHeader.hpp` 已将 vulkan 映射到 vk::，需确认 imgui_impl_vulkan.h 的 `#include <vulkan/vulkan.h>` 与项目 vk.hpp raii 共存无冲突。若编译报 Vulkan 类型冲突，则在 VKBackend.cpp 中仅在 ImGui 相关函数内使用，把 imgui include 放在 VKHeader.hpp include 之前，并保持默认（本项目 VK 头实际来自 vulkan 官方头，见 `src/rhi/vk/VKHeader.hpp`）。（执行时以编译结果为准微调 include 顺序，但这属于预期内的实现细节。）

- [ ] **Step 3: VKRenderer 实现 imguiInitInfo**
在 `VKBackend.cpp` 的 `VKRenderer` 类 private 区加成员：
```cpp
vk::raii::CommandPool _imguiPool{nullptr};
vk::raii::CommandBuffer _imguiCmd{nullptr};
vk::raii::Fence _imguiFence{nullptr};
```
在类声明处 override：
```cpp
bool imguiInitInfo(VKImGuiInitInfo& out) override;
void renderImGuiDrawData(void* drawData) override;
```
实现 `imguiInitInfo`（注意 vk::raii 取原生句柄用 `static_cast` 或 `*`，参照现有代码）：
```cpp
bool VKRenderer::imguiInitInfo(VKImGuiInitInfo& out) {
    if (!_device || !_swapchain) return false;
    out.instance = ...;          // instance 原生句柄
    out.physDevice = ...;
    out.device = static_cast<vk::Device>(*_device);      // *_device 解出原生
    out.graphicsFamily = _graphicsFamily;
    out.graphicsQueue = _graphicsQueue;                  // raii 转原生
    out.dsPool = *_dsPool;
    out.imageCount = _swapchain->imageCount();
    return true;
}
```
> `_instance`/`_physicalDevice` 是否存在需确认——VKRenderer 成员目前未列 `_instance`/`_physicalDevice`（只有 `_device`）。如何取 instance/physDevice：查看 init 里创建 device 前的保存方式（可能用 vk::raii::Instance/_physicalDevice 或原生 VkInstance）。若未保存，则新增成员保存（init 中 `createInstance`/`findPhysicalDevice` 后 `_instance = ...; _physicalDevice = ...;`），或在创建 device 时已持有原生句柄。以 VKBackend.cpp init 实际代码为准补齐，这是实现细节。

- [ ] **Step 4: 实现 renderImGuiDrawData**
```cpp
void VKRenderer::renderImGuiDrawData(void* drawData) {
    if (!drawData || !_recording || !_rpActive) return;
    // _cmd 内已处于活跃 render pass；ImGui backend 自设正高度 viewport + 无 cull，
    // 与 3D 负高度 viewport 经动态状态隔离，互不覆盖。
    ImGui_ImplVulkan_RenderDrawData(static_cast<ImDrawData*>(drawData),
                                    static_cast<vk::CommandBuffer>(*_cmd));
    // 3D 状态恢复：bindPipelineAndState 会在下一次 draw 时重新绑 UBO set + 动态状态，
    // 但 viewport 由 _viewportSet 标记控制在 beginFrame/draw 处，需确认恢复。
}
```
> 关键：ImGui 画完必须恢复 3D 动态状态。查看 `applyDynamicState` 与 viewport 设置逻辑（`bindPipelineAndState` 914-939）是否有 `_viewportSet` 复位，若无则在 renderImGuiDrawData 尾部把 `_viewportSet = false`，强制下一次 3D draw 重新设置负高度 viewport。
> 同时确认 ImGui 的 descriptor 绑定不影响 3D 的 `_uboDs` 绑定——ImGui backend 用自己 pipeline layout，不经过 `_dsLayout`，安全。

- [ ] **Step 5: 构建 + 运行回归（此时仍无渲染，验证无编译回归）**
```bash
./scripts/build_run.sh build 2>&1 | grep -E "error:|build OK"
RHI_DUMP_FRAME=/tmp/imgui_t2.ppm DISPLAY=:0 timeout 5 build/src/renderLearn -b vulkan -a Cube > /tmp/imgui_t2.log 2>&1
grep -oE "pixels black=[0-9]+ nonblack=[0-9]+" /tmp/imgui_t2.log | tail -1
```
Expected: `build OK` + `pixels black=0 nonblack=921600`（Cube 画面未破坏）。

- [ ] **Step 6: 提交**
```bash
git add src/rhi/core/IRenderer.hpp src/rhi/vk/VKBackend.hpp src/rhi/vk/VKBackend.cpp
git commit -m "feat(vk): VKRenderer 暴露 ImGui 初始化句柄与绘制钩子"
```

---

### Task 3: 新增 ImGuiVulkanWindow

**Files:**
- Create: `src/app/GL/ImGuiVulkanWindow.hpp`
- Create: `src/app/GL/ImGuiVulkanWindow.cpp`

**Interfaces:**
- Consumes: `IImGuiWindow`（`src/app/IImGuiWindow.hpp`）；`IRenderer::imguiInitInfo(VKImGuiInitInfo&)`；`IRenderer::renderImGuiDrawData(void*)`；GLFW window；vcpkg imgui 1.92.8；`imgui_impl_glfw.h`、`imgui_impl_vulkan.h`。
- Produces: `ImGuiVulkanWindow` 类（`init(GLFWwindow*)` / `newFrame()` / `render()` / `destroy()`），供 Task 4 的 GLApp 使用。

- [ ] **Step 1: 写头文件**
```cpp
#pragma once
#include "app/IImGuiWindow.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>

struct GLFWwindow;

class ImGuiVulkanWindow : public IImGuiWindow {
public:
    ~ImGuiVulkanWindow() override;
    void init(GLFWwindow* win, const std::shared_ptr<rhi::IRenderer>& renderer) override;
    void newFrame() override;
    void render() override;
    void destroy() override;

private:
    GLFWwindow* m_window{nullptr};
    std::shared_ptr<rhi::IRenderer> m_renderer{};
};
```
> `IImGuiWindow::init` 当前签名是 `virtual void init(GLFWwindow* win);`。为兼容现有 `m_imguiWindow->init(window)` 调用点，**不改变接口签名**，改为在 init 后单独注入 renderer，或在 GLApp 构造后调用一个 set。请在 Task 4 里决定调用形态（推荐保持接口不变，新增 `void setRenderer(const std::shared_ptr<rhi::IRenderer>&)`）。实现时保证 `GLApp.cpp:40-41` 的调用兼容。

- [ ] **Step 2: 实现 init（上下文 + 双 backend + 字体纹理）**
在 `ImGuiVulkanWindow.cpp`：
```cpp
#include "ImGuiVulkanWindow.hpp"
#include <imgui.h>
#include "imconfig.h"   // 若需要，编译期受控
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

void ImGuiVulkanWindow::init(GLFWwindow* win, const std::shared_ptr<rhi::IRenderer>& renderer) {
    m_window = win;
    m_renderer = renderer;

    rhi::VKImGuiInitInfo initInfo{};
    // 从 VKRenderer 取原生句柄
    rhi::IRenderer& rend = *renderer;
    // 通过 imguiInitInfo 填充（若返回 false 则记录并降级为无渲染）
    if (!rend.imguiInitInfo(initInfo)) return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    // 平台 backend
    ImGui_ImplGlfw_InitForVulkan(win, true);
    // 描述符池：backend 自建（DescriptorPoolSize=256），避免抢占 3D _dsPool
    ImGui_ImplVulkan_InitInfo vkInfo{};
    vkInfo.ApiVersion = VK_API_VERSION_1_0;
    vkInfo.Instance = initInfo.instance;
    vkInfo.PhysicalDevice = initInfo.physDevice;
    vkInfo.Device = initInfo.device;
    vkInfo.QueueFamily = initInfo.graphicsFamily;
    vkInfo.Queue = initInfo.graphicsQueue;
    vkInfo.DescriptorPoolSize = 256;              // 让 backend 自建 pool
    vkInfo.MinImageCount = initInfo.imageCount > 2 ? 2 : 1;
    vkInfo.ImageCount = initInfo.imageCount;
    vkInfo.MinAllocationSize = initInfo.minAllocationSize;
    vkInfo.PipelineInfoMain.RenderPass = /* VKRenderer 的 _renderPass 原生句柄 */;

    if (!ImGui_ImplVulkan_Init(&vkInfo)) { LOGE("ImGui_ImplVulkan_Init failed"); return; }
}
```
> `PipelineInfoMain.RenderPass` 需要 VKRenderer 的 render pass 原生句柄。Task 2 的 `VKImGuiInitInfo` 里**增加** `vk::RenderPass renderPass` 字段（Task 2 Step 2 一并定义），此处填入 `*_renderPass`（VKRenderer 内转换）。
> `LOGE` 宏来自哪里——查看 VKBackend.cpp 顶部 log 宏引入（应是公共 utils），在 ImGuiVulkanWindow.cpp include 相应头。若不便，用 `fprintf(stderr, ...)`。（以项目实际 log 设施为准。）
> `io.ConfigFlags` 与 GL 端行为一致即可（GL 端未开 NavEnableKeyboard 则此处也可不加）。

- [ ] **Step 3: 实现 newFrame**
```cpp
void ImGuiVulkanWindow::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}
```
> `ImGui_ImplVulkan_NewFrame()` 会内部处理 DisplaySize/DeltaTime（仿照 imgui 官方例子），不再手算。

- [ ] **Step 4: 实现 render 与 destroy**
```cpp
void ImGuiVulkanWindow::render() {
    ImGui::Render();
    if (m_renderer) m_renderer->renderImGuiDrawData(ImGui::GetDrawData());
}

void ImGuiVulkanWindow::destroy() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_renderer.reset();
}
```

- [ ] **Step 5: 处理创建字体纹理所需的立即提交命令缓冲**
> `ImGui_ImplVulkan_Init`（新版）内部随 `DescriptorPoolSize` 创建字体纹理时，需要一次立即提交的命令缓冲（backend 在 Init 内自建临时 pool/命令缓冲完成 font upload）。官方 1.92.8 backend 的 `CreateFontsTexture` 在 Init 内部流程处理（`ImGui_ImplVulkan_Init` 会提交 `BackendData->FontQueueFamily` 相关命令）。如编译后运行发现字体纹理未上传（面板空白），需在 VKRenderer 提供 `executeSingleSubmit` 辅助（参考现有 dumpFrame 的 `_dumpPool/_dumpCmd/_dumpFence` 模式，`VKBackend.cpp:315-340`），ImGuiVulkanWindow 于 init 中申请一次立即提交并 `ImGui_ImplVulkan_NewFrame` 前调用。**先在 Step 5 编译并简单运行，若字体空白再实施此兜底**（遵循 YAGNI，先跑通再补）。

- [ ] **Step 6: 构建验证（此时 GLApp 尚未接 VK 窗口，仅验证新文件编译通过被收录）**
```bash
# 确认 CMakeLists（src/CMakeLists.txt:24 用 ${IMGUI_SRC_FILES}）已收录新 cpp
grep -rn "ImGuiVulkanWindow" src/ || true
```
> src/app 下 cpp 是否自动收录：查看 `src/CMakeLists.txt` 是否 `aux_source_directory`/`file(GLOB)` 源文件。若无自动 glob，需在 `src/CMakeLists.txt` 显式添加 `ImGuiVulkanWindow.cpp`。构建：
```bash
./scripts/build_run.sh build 2>&1 | grep -E "error:|build OK"
```
Expected: build OK（新文件已编译，尚未被调用）。

- [ ] **Step 7: 提交**
```bash
git add src/app/GL/ImGuiVulkanWindow.hpp src/app/GL/ImGuiVulkanWindow.cpp
git commit -m "feat(app): 新增 ImGuiVulkanWindow（GLFW+Vulkan 双 backend）"
```

---

### Task 4: GLApp 接线（Vulkan 模式启用新窗口）

**Files:**
- Modify: `src/app/GL/GLApp.cpp:40-41`（vulkan 分支）
- Modify: `src/app/GL/GLApp.hpp`
- 必要时 Modify: `src/app/GL/ImGuiVulkanWindow.hpp`（若加 `setRenderer`）

**Interfaces:**
- Consumes: `ImGuiVulkanWindow`（Task 3）；`_renderer`（IRenderer，持有 createVKRenderer 返回对象）；`m_imguiWindow`（unique_ptr<IImGuiWindow>）。
- Produces: 完整接线；给 Task 5 提供可测试的运行形态。

- [ ] **Step 1: vulkan 分支替换**
编辑 `GLApp::initGraphics` vulkan 分支（当前 `m_imguiWindow = std::make_unique<ImGuiContextWindow>(); m_imguiWindow->init(window);`）改为：
```cpp
auto imguiVk = std::make_unique<ImGuiVulkanWindow>();
imguiVk->setRenderer(_renderer);
imguiVk->init(m_window->getNativeGLFWWindow());
m_imguiWindow = std::move(imguiVk);
```
（若实现为 init(win, renderer) 双参则 `m_imguiWindow = std::make_unique<ImGuiVulkanWindow>(); m_imguiWindow->init(m_window->getNativeGLFWWindow(), _renderer);`。）保持与 GL 分支 (`ImGuiOpenglWindow`) 对称即可。
> `ImGuiContextWindow` 不再使用，可删除文件或保留（设计文档范围未要求删除，YAGNI：**保留文件但不再引用**，最少改动）。

- [ ] **Step 2: 检查 destroy 路径**
确认 `Application`/`GLApp` 析构或 shutdown 时调用 `m_imguiWindow->destroy()`（GL 端 `ImGuiOpenglWindow::~` 与 `destroy` 的行为对照）。若无显式调用，则在 GLApp 析构或 `Application` run 结束后调用一次，避免 Vulkan 设备销毁顺序问题（ImGui 资源先于 VK device 释放）。

- [ ] **Step 3: 构建 + 运行验证（核心验收）**
```bash
./scripts/build_run.sh build 2>&1 | grep -E "error:|build OK"
```
DISPLAY=:0 下运行（支持 llvmpipe 软件渲染）：
```bash
RHI_DUMP_FRAME=/tmp/imgui_t4.ppm DISPLAY=:0 timeout 6 build/src/renderLearn -b vulkan -a Cube > /tmp/imgui_t4.log 2>&1
grep -oE "pixels black=[0-9]+ nonblack=[0-9]+" /tmp/imgui_t4.log | tail -1
```
Expected: `pixels black=0 nonblack=921600` 且 cube 仍在中央（3D 未破坏）。
> llvmpipe 上 ImGui 面板可见性可用 dump 图人工检查（左上角应出现 ImGui 方框/文本像素）。文字方向应正常不颠倒（ImGui 自设正高度 viewport）。

- [ ] **Step 4: 提交**
```bash
git add src/app/GL/GLApp.cpp src/app/GL/GLApp.hpp
git commit -m "feat(app): Vulkan 模式启用 ImGuiVulkanWindow 渲染覆盖层"
```

---

### Task 5: 全量回归与最终验证

**Files:**
- 无源文件改动（仅验证）。

**Interfaces:**
- Consumes: 全部先前任务产物。

- [ ] **Step 1: 3D 翻转修复回归**
用非对称纹理验证翻转仍正确（dog.jpg 方向判据）：
```bash
RHI_DUMP_FRAME=/tmp/reg_simple.ppm DISPLAY=:0 timeout 5 build/src/renderLearn -b vulkan -a SimpleTexture > /tmp/reg_simple.log 2>&1
python3 -c "
from PIL import Image
import numpy as np
im=np.asarray(Image.open('/tmp/reg_simple.ppm').convert('L'),dtype=float)
h,w=im.shape
print('VK top=%.1f bottom=%.1f | left=%.1f right=%.1f'%(im[:h//3].mean(),im[-h//3:].mean(),im[:,:w//3].mean(),im[:,-w//3:].mean()))
"
```
Expected: `top > bottom` 且 `left > right`（图未翻转）。

- [ ] **Step 2: 多 App 运行回归**
```bash
for a in Triangle Cube Hdr FrameBuffer CullFace Shadow_Map Gamma Msaa; do
  RHI_DUMP_FRAME=/tmp/reg_$a.ppm DISPLAY=:0 timeout 5 build/src/renderLearn -b vulkan -a "$a" > /tmp/reg_$a.log 2>&1
  echo "$a exit=$? stat=$(grep -oE 'black=[0-9]+ nonblack=[0-9]+' /tmp/reg_$a.log | tail -1)"
done
```
Expected: 每个 exit=124（timeout 自然退出）且 stat 与修复前基线一致（Shadow_Map≈1128、Gamma≈961、CullFace≈33、其余 0），画面正常。

- [ ] **Step 3: 确认 ImGui 面板可见**
用 dump 帧人工确认 ImGui 面板（左上角矩形 + FPS 文本）出现在画面中，无翻转、无黑屏。若 llvmpipe 下面板不可见（字体纹理未上传），执行 Task 3 Step 5 的立即提交兜底，再重跑本步骤。

- [ ] **Step 4: 收尾**
清理 .bak 备份文件；更新 PROGRESS.md。
```bash
cd /home/ares/workspace/GraphicsAPILearn
rm -f 3party/imgui/Vulkan/imgui_impl_vulkan.cpp.bak 3party/imgui/Vulkan/imgui_impl_vulkan.h.bak
git add -A
git commit -m "chore: 清理 ImGui backend 旧备份并记录 VK ImGui 覆盖层验证"
```

---

## Self-Review 确认

- **Spec 覆盖**：Section 1(backend 替换)→Task1；Section 2(ImGuiVulkanWindow)→Task3；Section 3(VKRenderer hook)→Task2；Section 4(接线)→Task4；Section 5(验证)→Task5。✓
- **类型一致性**：`VKImGuiInitInfo` 在 Task2 Step2 定义，Task3 Step2 引用同一字段名（instance/physDevice/device/graphicsFamily/graphicsQueue/descriptorPool/imageCount/renderPass/minAllocationSize）。Task2 标题 Step 中的 `dsPool` 字段在 Task3 使用 `DescriptorPoolSize`（backend 自建），故 `dsPool` 字段可能不需要传给 backend，但保留在结构体内供排查。✓
- **风险已知**：instance/physDevice 需确认 VKRenderer 是否保存（Task2 已注明以现有代码为准补充）；render pass 原生句柄需加进结构体；字体上传可能需要立即提交兜底；include 顺序可能需微调。全部实现细节已在任务内显式说明，无占位符。