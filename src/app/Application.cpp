#include <iostream>

#include <cassert>
#include <sstream>
#include <chrono>
#include <thread>

#include "Application.hpp"
#include "base/ErrorHandle.hpp"
#include "base/Log.hpp"
#include <imgui.h>
#include "utils/EventUtils.hpp"

namespace eh = ErrorHandle;
using namespace Utils::Event;
Application::Application() {}

Application::~Application() {
    if (m_window) {
        m_window->shutdown();
    }

    if (m_imguiWindow) {
        m_imguiWindow->destroy();
    }
}

bool Application::init(const GLFWWindowProperties& properties){
    m_window = std::make_unique<GLFWWindow>(properties);
    if(!m_window->initialize()){
        LOGE("GLFWWindow init failed");
        return false;
    }
    
    LOGI("Create Main Windows successed");
    LOGI("Window title: {}", properties.title.c_str());
    LOGI("Window properties: pos({}, {}), size({}, {})", properties.xPos, properties.yPos, properties.width, properties.height);
    m_window->setUserPointer(this);
    initInputEvent();
    if(!initGraphics()){
        LOGE("initGraphics failed");
        return false;
    }

    if(!initApp()){
        LOGE("initApp failed");
        return false;
    }
    
    return true;
}

float Application::aspectRatio() const {
    const auto prop = m_window->getProperties();
    return static_cast<float>(prop.width) / static_cast<float>(prop.height);
}

static auto CalcWindowFrameRate() {
    static auto lastTime = std::chrono::high_resolution_clock::now();
    static int frameCount = 0;
    static float fps = 0.0f;

    auto currentTime = std::chrono::high_resolution_clock::now();
    frameCount++;
    std::chrono::duration<float> elapsed = currentTime - lastTime;
    if (elapsed.count() >= 0.5f) {
        fps = frameCount / elapsed.count();  
        frameCount = 0;                      
        lastTime = currentTime;              
    }

    return fps;
}

void Application::onKeyPress(int key, int scancode, int action, int mods){
    LOGI("Key press: {}, {}, {}, {}", key, scancode, action, mods);
    const auto keyCode = ConvertKeyCode(key);
    const auto keyAction = ConvertKeyAction(action);
    if(keyCode == Key::Esc && keyAction == KeyAction::Press){
        exit();
    }
}

void Application::onMouseMove(double x, double y){
    //LOGI("Mouse move: {}, {}", x, y);
}

void Application::onMouseScroll(double xoffset, double yoffset){
    LOGI("Mouse scroll: {}, {}", xoffset, yoffset);
}

void Application::onMouseButton(int button, int action, int mods){
    LOGI("Mouse button: {}, action: {}, mods: {}", button, action, mods);
}
void Application::onWindowResize(int width, int height){
    LOGI("Window resize: {} {}", width, height);
}

void Application::initInputEvent(){
    m_window->setKeyCallback([this](int key, int scancode, int action, int mods){
        onKeyPress(key, scancode, action, mods);
    }).setMouseMoveCallback([this](double x, double y){
        onMouseMove(x, y);
    }).setMouseScrollCallback([this](double xoffset, double yoffset){
        onMouseScroll(xoffset, yoffset);
    }).setMouseButtonCallback([this](int button, int action, int mods){
        onMouseButton(button, action, mods);
    }).setWindowResizeCallback([this](int width, int height){
        onWindowResize(width, height);
    });
}

void Application::updateFrameRate(){
    const auto frate = CalcWindowFrameRate();
    m_window->updateFrameRate(frate);
}

int Application::run(){
    renderBeforeLoop();
    while (!m_window->shouldClose()) {
        updateFrameRate();
        m_window->beginFrame();
        m_window->pollEvents();
        render();
        m_window->endFrame();
        std::this_thread::yield();
    }

    return 0;
}

void Application::exit() {
    if(m_window){
        m_window->setShouldClose(true);
    }
}

void Application::render() {
    beginDrawScene();
    m_imguiWindow->newFrame();
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);
    drawScene(elapsed.count());
    lastTime = currentTime;
    m_imguiWindow->render();
    endDrawScene();
}

void Application::beginDrawScene() {
    clearColor();
    return;
}