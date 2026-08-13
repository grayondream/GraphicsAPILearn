#include "GLApp.hpp"
#include "base/ErrorHandle.hpp"
#include "base/Log.hpp"
#include "ImGuiOpenglWindow.hpp"
#include "rhi/gl/GLBackend.hpp"
#include "rhi/gl/GLFWSurface.hpp"
#if ENABLE_VULKAN
#include "rhi/vk/VKBackend.hpp"
#endif
#include <imgui.h>

using namespace ErrorHandle;

GLApp::GLApp() {

}

GLApp::~GLApp() {

}

bool GLApp::initGraphics() {
    const auto props = m_window->getProperties();

#if ENABLE_VULKAN
    if (props.vulkan) {
        rhi::setBackendKind(rhi::BackendKind::Vulkan);
        auto surface = std::make_shared<rhi::GLFWSurface>(
            m_window->getNativeGLFWWindow(), props.width, props.height);
        _renderer = rhi::createVKRenderer();
        if (!_renderer->init(surface)) {
            LOGE("Failed to init Vulkan renderer");
            return false;
        }
        _renderer->setViewport(rhi::Viewport{0, 0, static_cast<int>(props.width), static_cast<int>(props.height)});
        _renderer->setPipeline(nullptr);
        return true;
    }
#else
    (void)props;
#endif

    if (!m_window->initGLContext()) {
        LOGE("Failed to initialize GL Context");
        return false;
    }

    auto surface = std::make_shared<rhi::GLFWSurface>(
        m_window->getNativeGLFWWindow(), props.width, props.height);
    _renderer = rhi::createGLRenderer();
    if (!_renderer->init(surface)) {
        LOGE("Failed to init renderer");
        return false;
    }
    _renderer->setViewport(rhi::Viewport{0, 0, props.width, props.height});
    _renderer->setPipeline(nullptr);

    m_imguiWindow = std::make_unique<ImGuiOpenglWindow>();
    m_imguiWindow->init(m_window->getNativeGLFWWindow());
    return true;
}

void GLApp::clearColor() {
    _renderer->clearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

void GLApp::beginDrawScene() {
    return Application::beginDrawScene();
}

void GLApp::drawScene(const float dt) {
    if (m_imguiWindow) {
        ImGui::Begin("OpenGL");
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("Hello Graphic! %.1f FPS", io.Framerate);  // Display current FPS
        ImGui::End();
    }

    return Application::drawScene(dt);
}

void GLApp::endDrawScene() {
    _renderer->present();
    return Application::endDrawScene();
}
