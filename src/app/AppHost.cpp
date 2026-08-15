#include "AppHost.hpp"
#include "base/ErrorHandle.hpp"
#include "base/Log.hpp"
#include "GL/ImGuiOpenglWindow.hpp"
#include "rhi/gl/GLBackend.hpp"
#include "rhi/gl/GLFWSurface.hpp"
#include <imgui.h>
#include <chrono>
#include <thread>

using namespace ErrorHandle;

AppHost::AppHost(std::shared_ptr<Sample> sample)
    : _sample(std::move(sample)) {}

AppHost::~AppHost() {
    if (_window) {
        _window->shutdown();
    }
    if (_imguiWindow) {
        _imguiWindow->destroy();
    }
}

bool AppHost::init(const GLFWWindowProperties& properties) {
    auto props = properties;
    if (props.samples == 0) {
        props.samples = getSampleCount();
    }

    _window = std::make_unique<GLFWWindow>(props);
    if (!_window->initialize()) {
        LOGE("GLFWWindow init failed");
        return false;
    }
    _window->setUserPointer(this);

    if (!_window->initGLContext()) {
        LOGE("Failed to initialize GL Context");
        return false;
    }

    auto surface = std::make_shared<rhi::GLFWSurface>(
        _window->getNativeGLFWWindow(), props.width, props.height);
    _renderer = rhi::createGLRenderer();
    if (!_renderer->init(surface)) {
        LOGE("Failed to init renderer");
        return false;
    }
    _renderer->setViewport(rhi::Viewport{0, 0, static_cast<int>(props.width), static_cast<int>(props.height)});
    _renderer->setPipeline(nullptr);

    _imguiWindow = std::make_unique<ImGuiOpenglWindow>();
    _imguiWindow->init(_window->getNativeGLFWWindow());

    initInput();

    _sample->setWindowSize(props.width, props.height);
    _sample->setRenderer(_renderer);
    if (_sample && !_sample->load(_renderer)) {
        LOGE("Sample load failed");
        return false;
    }
    return true;
}

void AppHost::initInput() {
    auto s = _sample;
    _window->setKeyCallback([s](int key, int scancode, int action, int mods) {
        s->onKeyPress(key, scancode, action, mods);
    }).setMouseMoveCallback([s](double x, double y) {
        s->onMouseMove(x, y);
    }).setMouseScrollCallback([s](double xoffset, double yoffset) {
        s->onMouseScroll(xoffset, yoffset);
    }).setMouseButtonCallback([s](int button, int action, int mods) {
        s->onMouseButton(button, action, mods);
    }).setWindowResizeCallback([s](int width, int height) {
        s->onWindowResize(width, height);
    });
}

unsigned int AppHost::getSampleCount() const {
    return _sample ? _sample->getSampleCount() : 0;
}

int AppHost::run() {
    if (_sample) {
        _sample->renderBeforeLoop();
    }
    while (!_window->shouldClose()) {
        _window->beginFrame();
        _window->pollEvents();

        _renderer->beginFrame();
        _renderer->clearColor(0.1f, 0.1f, 0.1f, 1.0f);

        if (_imguiWindow) {
            _imguiWindow->newFrame();
        }
        if (_sample) {
            static auto lastTime = std::chrono::high_resolution_clock::now();
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);
            lastTime = currentTime;
            _sample->draw(elapsed.count());
            ImGui::Begin("OpenGL");
            ImGuiIO& io = ImGui::GetIO();
            ImGui::Text("Hello Graphic! %.1f FPS", io.Framerate);
            ImGui::End();
        }
        if (_imguiWindow) {
            _imguiWindow->render();
        }

        _renderer->endFrame();
        _renderer->present();
        _window->endFrame();
        std::this_thread::yield();
    }
    return 0;
}

void AppHost::exit() {
    if (_window) {
        _window->setShouldClose(true);
    }
}
