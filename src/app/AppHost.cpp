#include "AppHost.hpp"
#include "GLFWWindow.hpp"
#include "IImGuiWindow.hpp"
#include "app/sample/Sample.hpp"
#include "app/SampleFactory.hpp"
#include "app/GL/ImGuiOpenglWindow.hpp"
#include "app/GL/ImGuiVulkanWindow.hpp"
#include "rhi/gl/GLBackend.hpp"
#include "rhi/gl/GLFWSurface.hpp"
#include "rhi/core/Common.hpp"
#if ENABLE_VULKAN
#include "rhi/vk/VKBackend.hpp"
#endif
#include "base/ErrorHandle.hpp"
#include "base/Log.hpp"
#include <imgui.h>
#include <chrono>
#include <thread>

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
    if (!m_window->initGLContext()) { LOGE("Failed to initialize GL Context"); return false; }
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
    auto s = SampleFactory::create(_sampleType);
    if (!s) { LOGE("SampleFactory::create failed for type {}", static_cast<int>(_sampleType)); return false; }
    const auto props = m_window ? m_window->getProperties() : GLFWWindowProperties();
    s->setWindowSize(props.width, props.height);
    s->setRenderer(_renderer);
    if (!s->load(_renderer)) { LOGE("Sample load failed for type {}", static_cast<int>(_sampleType)); return false; }
        _sample = std::move(s);
        return true;
}

bool AppHost::init(const GLFWWindowProperties& properties) {
    auto props = properties;
    if (props.samples == 0) props.samples = getSampleCount();
    props.vulkan = (_backend == GraphicsType::Vulkan);

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
            props.vulkan = (_backend == GraphicsType::Vulkan);
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

void AppHost::renderTotalBar() {
    if (!m_imguiWindow) return;
    ImGui::Begin("OpenGL");
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Hello Graphic! %.1f FPS", io.Framerate);
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
    if (_sample) _sample->renderBeforeLoop();
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
            _sample->draw(static_cast<float>(dt));
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
