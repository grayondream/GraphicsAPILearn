#include "AppHost.hpp"
#include "GLFWWindow.hpp"
#include "IImGuiWindow.hpp"
#include "app/sample/Sample.hpp"
#include "app/SampleFactory.hpp"
#include "app/Samples/ImGuiOpenglWindow.hpp"
#include "app/Samples/ImGuiVulkanWindow.hpp"
#include "app/Samples/ImGuiDirectx12Window.hpp"
#include "app/Samples/ImGuiDirectx11Window.hpp"
#include "rhi/gl/GLBackend.hpp"
#include "rhi/gl/GLFWSurface.hpp"
#include "rhi/core/Common.hpp"
#if ENABLE_VULKAN
#include "rhi/vk/VKBackend.hpp"
#endif
#if ENABLE_DX12
#include "rhi/dx12/DXBackend.hpp"
#endif
#if ENABLE_DX11
#include "rhi/dx11/DX11Backend.hpp"
#endif
#include "base/ErrorHandle.hpp"
#include "base/Log.hpp"
#include "utils/EnumUtil.hpp"
#include <imgui.h>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace ErrorHandle;

AppHost::AppHost() = default;

AppHost::~AppHost() {
    destroyBackendResources();
    if (m_window) m_window->shutdown();
}

void AppHost::destroyBackendResources() {
    _sample.reset();
    if (m_imguiWindow) { m_imguiWindow->destroy(); m_imguiWindow.reset(); }
    _renderer.reset();
}

bool AppHost::rebuildBackend(const GLFWWindowProperties& props) {
    destroyBackendResources();
    if (!m_window) return false;
#if ENABLE_VULKAN
    if (_backend == GraphicsType::Vulkan) {
        // 后端切换需按新 client API 重建原生窗口（GLFW_CLIENT_API 在 initialize 内决定）
        m_window->shutdown();
        m_window->initialize();
        rhi::setBackendKind(rhi::BackendKind::Vulkan);
        auto surface = std::make_shared<rhi::GLFWSurface>(
            m_window->getNativeGLFWWindow(), static_cast<int>(props.width), static_cast<int>(props.height));
        _renderer = rhi::createVKRenderer();
        if (!_renderer->init(surface)) { LOGE("Failed to init Vulkan renderer"); return false; }
        _renderer->setViewport(rhi::Viewport{0, 0, static_cast<int>(props.width), static_cast<int>(props.height)});
        _renderer->setPipeline(nullptr);
        auto imguiVk = std::make_unique<ImGuiVulkanWindow>();
        imguiVk->setRenderer(_renderer);
        imguiVk->init(m_window->getNativeGLFWWindow());
        m_imguiWindow = std::move(imguiVk);
        hookWindowCallbacks();
        return reloadSample();
    }
#else
    (void)props;
#endif
#if ENABLE_DX12
    if (_backend == GraphicsType::DX12) {
        m_window->shutdown();
        m_window->initialize();
        rhi::setBackendKind(rhi::BackendKind::Dx12);
        auto surface = std::make_shared<rhi::GLFWSurface>(
            m_window->getNativeGLFWWindow(), static_cast<int>(props.width), static_cast<int>(props.height));
        _renderer = rhi::createDX12Renderer();
        if (!_renderer->init(surface)) { LOGE("Failed to init DX12 renderer"); return false; }
        _renderer->setViewport(rhi::Viewport{0, 0, static_cast<int>(props.width), static_cast<int>(props.height)});
        _renderer->setPipeline(nullptr);
        auto imguiDx = std::make_unique<ImGuiDirectx12Window>();
        imguiDx->setRenderer(_renderer);
        imguiDx->init(m_window->getNativeGLFWWindow());
        m_imguiWindow = std::move(imguiDx);
        hookWindowCallbacks();
        return reloadSample();
    }
#endif
#if ENABLE_DX11
    if (_backend == GraphicsType::DX11) {
        // 后端切换需按新 client API 重建原生窗口：D3D11 flip 交换链要求 HWND 未被
        // GDI/OpenGL 设置像素格式（与 VK 同理走 NO_API，见 GLFWWindow::initialize）
        m_window->shutdown();
        m_window->initialize();
        rhi::setBackendKind(rhi::BackendKind::Dx11);
        auto surface = std::make_shared<rhi::GLFWSurface>(
            m_window->getNativeGLFWWindow(), static_cast<int>(props.width), static_cast<int>(props.height));
        _renderer = rhi::createDX11Renderer();
        if (!_renderer || !_renderer->init(surface)) { LOGE("Failed to init DX11 renderer"); return false; }
        _renderer->setViewport(rhi::Viewport{0, 0, static_cast<int>(props.width), static_cast<int>(props.height)});
        _renderer->setPipeline(nullptr);
        // Task 6：真 overlay（对照 DX12 分支）——GetDX11ImGuiInitInfo 桥取
        // device/context，总控条可见可操作；此前为 ImGuiContextWindow 兜底
        // （仅建 context 不渲染 DrawData）
        auto imguiDx11 = std::make_unique<ImGuiDirectx11Window>();
        imguiDx11->setRenderer(_renderer);
        imguiDx11->init(m_window->getNativeGLFWWindow());
        m_imguiWindow = std::move(imguiDx11);
        hookWindowCallbacks();
        return reloadSample();
    }
#endif
    // 后端切换需按新 client API 重建原生窗口；VK 分支在上面已重建为 NO_API，
    // GL 分支同样需 shutdown+initialize 以按属性重建为 OPENGL 上下文窗口。
    if (_backend == GraphicsType::GL) { m_window->shutdown(); m_window->initialize(); }
    rhi::setBackendKind(rhi::BackendKind::GL);
    if (!m_window->initGLContext()) { LOGE("Failed to initialize GL Context (backend={})", static_cast<int>(_backend)); return false; }
    auto surface = std::make_shared<rhi::GLFWSurface>(
        m_window->getNativeGLFWWindow(), static_cast<int>(props.width), static_cast<int>(props.height));
    _renderer = rhi::createGLRenderer();
    if (!_renderer->init(surface)) { LOGE("Failed to init renderer"); return false; }
    _renderer->setViewport(rhi::Viewport{0, 0, static_cast<int>(props.width), static_cast<int>(props.height)});
    _renderer->setPipeline(nullptr);
    m_imguiWindow = std::make_unique<ImGuiOpenglWindow>();
    m_imguiWindow->init(m_window->getNativeGLFWWindow());
    return reloadSample();
}

bool AppHost::reloadSample() {
    // 切换样例前先等待 GPU 空闲：上一帧的 command buffer 可能仍在执行，其引用的
    // 旧样例 UBO/纹理/描述符集在下方创建新样例、随后销毁旧样例时会被释放/改写。
    // 若不等空闲就销毁（Vulkan 需等 fence 后才能释放仍被提交命令引用的资源），
    // llvmpipe 工作线程会在读取已释放的 buffer/descriptor 内存时崩溃（用户观察到：
    // 从 Triangle 切到 Mult 不崩、从其他光源样例切到 Mult 必崩——光源样例含纹理/UBO）。
    if (_renderer) _renderer->waitIdle();
    if (_renderer) _renderer->resetRenderState();  // 清除上一样例残留的全局渲染状态(GL 深度测试等)
    auto s = SampleFactory::create(_sampleType);
    if (!s) { LOGE("SampleFactory::create failed for type {}", static_cast<int>(_sampleType)); return false; }
    const auto props = m_window ? m_window->getProperties() : GLFWWindowProperties();
    s->setWindowSize(props.width, props.height);
    s->setRenderer(_renderer);
    if (!s->load(_renderer)) { LOGE("Sample load failed for type {}", static_cast<int>(_sampleType)); return false; }
    s->renderBeforeLoop();
        _sample = std::move(s);
        return true;
}

bool AppHost::init(const GLFWWindowProperties& properties) {
    auto props = properties;
    if (props.samples == 0) props.samples = getSampleCount();
    // DX11 与 VK 同样不需要 GL 上下文（NO_API 窗口，D3D11 自建交换链）
    props.vulkan = (_backend == GraphicsType::Vulkan || _backend == GraphicsType::DX11);

    m_window = std::make_unique<GLFWWindow>(props);
    if (!m_window->initialize()) { LOGE("GLFWWindow init failed"); return false; }
    hookWindowCallbacks();
    if (!rebuildBackend(props)) return false;
    // init 已按 setSample/setBackend 完成初始后端与样例构建，清除预置的 pending 标记，
    // 避免首帧 applyPendingChanges 额外重建后端（重复重建 VK 后端会触发其 teardown 顺序问题）。
    _pendingSample = false;
    _pendingBackend = false;
    return true;
}

void AppHost::hookWindowCallbacks() {
    m_window->setUserPointer(this);
    m_window->setKeyCallback([this](int k, int s, int a, int m) { forwardKey(k, s, a, m); })
        .setMouseMoveCallback([this](double x, double y) { forwardMouseMove(x, y); })
        .setMouseScrollCallback([this](double x, double y) { forwardMouseScroll(x, y); })
        .setMouseButtonCallback([this](int b, int a, int m) { forwardMouseButton(b, a, m); })
        .setWindowResizeCallback([this](int w, int h) { forwardWinResize(w, h); });
}

void AppHost::setSample(AppType type) { _sampleType = type; _pendingSample = true; }
void AppHost::setBackend(GraphicsType type) { _backend = type; _pendingBackend = true; }

void AppHost::forwardKey(int key, int scancode, int action, int mods) {
    if (_sample) _sample->onKeyPress(key, scancode, action, mods);
}
void AppHost::forwardMouseMove(double x, double y) {
    if (_sample) _sample->onMouseMove(x, y);
}
void AppHost::forwardMouseScroll(double xoffset, double yoffset) {
    if (_sample) _sample->onMouseScroll(xoffset, yoffset);
}
void AppHost::forwardMouseButton(int button, int action, int mods) {
    if (_sample) _sample->onMouseButton(button, action, mods);
}
void AppHost::forwardWinResize(int width, int height) {
    if (_sample) _sample->onWindowResize(width, height);
}

void AppHost::applyPendingChanges() {
    if (_pendingSample) {
        _pendingSample = false;
        if (_renderer) reloadSample();
    }
    if (_pendingBackend) {
        _pendingBackend = false;
        if (m_window) {
            auto props = m_window->getProperties();
            props.vulkan = (_backend == GraphicsType::Vulkan || _backend == GraphicsType::DX11);
            m_window->setProperties(props);
            if (!rebuildBackend(props)) LOGE("Backend rebuild failed");
        }
    }
}

unsigned int AppHost::getSampleCount() const {
    auto s = SampleFactory::create(_sampleType);
    return s ? s->getSampleCount() : 0;
}

float AppHost::aspect() const {
    const auto p = m_window ? m_window->getProperties() : GLFWWindowProperties();
    return p.height != 0 ? static_cast<float>(p.width) / static_cast<float>(p.height) : 1.0f;
}

namespace {

std::string_view StripEnumPrefix(const std::string_view name) {
    const auto pos = name.rfind("::");
    return pos == std::string_view::npos ? name : name.substr(pos + 2);
}

std::string_view EnumMemberName(const auto v) {
    return StripEnumPrefix(Utils::Enum::EnumName(v));
}

} // namespace

void AppHost::renderTotalBar() {
    if (!m_imguiWindow) return;
    ImGuiIO& io = ImGui::GetIO();
    // 固定到右上角并置顶：样例窗口默认出现在左上角(如 Cube 的 Sample count 面板)，
    // 若不固定位置，会与总控条重叠并覆盖"后端/样例"选择 UI。
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 360.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_Always);
    ImGui::Begin("Control", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    // 后端 Combo —— 只列编译启用的后端
    std::vector<const char*> backendNames;
    std::vector<GraphicsType> backendTypes;
#if ENABLE_OPENGL
    backendNames.emplace_back("OpenGL");
    backendTypes.emplace_back(GraphicsType::GL);
#endif
#if ENABLE_VULKAN
    backendNames.emplace_back("Vulkan");
    backendTypes.emplace_back(GraphicsType::Vulkan);
#endif
#if ENABLE_DX12
    backendNames.emplace_back("Direct3D 12");
    backendTypes.emplace_back(GraphicsType::DX12);
#endif
#if ENABLE_DX11
    backendNames.emplace_back("Direct3D 11");
    backendTypes.emplace_back(GraphicsType::DX11);
#endif
    int curBackend = 0;
    for (int i = 0; i < static_cast<int>(backendTypes.size()); ++i)
        if (backendTypes[i] == _backend) curBackend = i;
    if (ImGui::Combo("Backend", &curBackend, backendNames.data(), static_cast<int>(backendNames.size()))) {
        if (backendTypes[curBackend] != _backend) setBackend(backendTypes[curBackend]);
    }

    ImGui::Separator();

    // 样例下拉 —— 名称顺序与 AppType.hpp 枚举定义顺序一致（0..Count）
    // 预分配容量：避免 vector 扩容使短名称(SSO)的 c_str() 指针悬垂导致乱码
    std::vector<std::string> sampleNameStorage;
    std::vector<const char*> sampleNames;
    sampleNameStorage.reserve(static_cast<size_t>(AppType::Count));
    sampleNames.reserve(static_cast<size_t>(AppType::Count));
    for (int i = 0; i < static_cast<int>(AppType::Count); ++i) {
        sampleNameStorage.emplace_back(EnumMemberName(static_cast<AppType>(i)));
        sampleNames.emplace_back(sampleNameStorage.back().c_str());
    }
    int curSample = static_cast<int>(_sampleType);
    if (ImGui::Combo("Sample", &curSample, sampleNames.data(), static_cast<int>(sampleNames.size()))) {
        if (static_cast<AppType>(curSample) != _sampleType) setSample(static_cast<AppType>(curSample));
    }

    ImGui::Separator();

    // 信息行：当前后端名 + 当前样例名 + FPS
    ImGui::Text("Backend: %s   Sample: %s   FPS: %.1f",
                std::string(EnumMemberName(currentBackend())).c_str(),
                std::string(EnumMemberName(currentSample())).c_str(), io.Framerate);

    ImGui::End();
}

static float CalcHostRate() {
    static auto last = std::chrono::high_resolution_clock::now();
    static int frames = 0;
    static float fps = 0.0f;
    auto now = std::chrono::high_resolution_clock::now();
    frames++;
    std::chrono::duration<float> e = now - last;
    if (e.count() >= 0.5f) { fps = frames / e.count(); frames = 0; last = now; }
    return fps;
}

int AppHost::run() {
    auto lastTime = std::chrono::high_resolution_clock::now();
    while (m_window && !m_window->shouldClose() && !_exitRequested) {
        const auto frate = CalcHostRate();
        m_window->updateFrameRate(frate);
        m_window->beginFrame();
        m_window->pollEvents();

        _renderer->clearColor(0.1f, 0.1f, 0.1f, 1.0f);
        _renderer->beginFrame();
        if (m_imguiWindow) m_imguiWindow->newFrame();
        if (_sample) {
            auto now = std::chrono::high_resolution_clock::now();
            auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();
            lastTime = now;
            // dt 为毫秒，样例按秒设计(如 angle=20*(i+1)*curTime 期望 20度/秒)。
            // 统一转为秒后传入，旋转等时间动画速度才合理。
            _sample->draw(static_cast<float>(dt) / 1000.0f);
        }
        renderTotalBar();
        if (m_imguiWindow) m_imguiWindow->render();
        _renderer->endFrame();
        _renderer->present();
        m_window->endFrame();

        applyPendingChanges();
        std::this_thread::yield();
    }
    return 0;
}

void AppHost::exit() {
    _exitRequested = true;
}
