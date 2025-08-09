#include <iostream>

#include <cassert>
#include <sstream>
#include <chrono>
#include <thread>

#include "Application.hpp"
#include "Base/ErrorHandle.hpp"
#include "Base/Log.hpp"
#include <imgui.h>
#include "Utils/EventUtils.hpp"

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

    return true;
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
    drawScene(0.0);
    m_imguiWindow->render();
    endDrawScene();
}

void Application::beginDrawScene() {
    clearColor();
    return;
}