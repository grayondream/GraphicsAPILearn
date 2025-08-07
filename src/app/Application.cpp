#include <iostream>

#include <cassert>
#include <sstream>
#include <chrono>
#include <thread>

#include "Application.hpp"
#include "EH/ErrorHandle.hpp"
#include "Base/Log.hpp"
#include <imgui.h>


namespace eh = ErrorHandle;

Application::Application() {}

Application::~Application() {
    if (m_window) {
        m_window->shutdown();
    }
}

bool Application::init(const GLFWWindowProperties& properties){
    m_window = std::make_unique<GLFWWindow>(properties);
    if(!m_window->initialize()){
        LOGI("GLFWWindow init failed");
        return false;
    }
    
    LOGI("Create Main Windows successed");
    LOGI("Window title: {}", properties.title.c_str());
    LOGI("Window properties: pos({}, {}), size({}, {})", properties.xPos, properties.yPos, properties.width, properties.height);
    return true;
}

static auto CalcWindowFrameRate() {
    static auto lastTime = std::chrono::high_resolution_clock::now();
    static int frameCount = 0;
    static float fps = 0.0f;

    auto currentTime = std::chrono::high_resolution_clock::now();
    frameCount++;

    // 计算时间差（秒）
    std::chrono::duration<float> elapsed = currentTime - lastTime;

    // 每0.5秒更新一次帧率，避免频繁波动
    if (elapsed.count() >= 0.5f) {
        fps = frameCount / elapsed.count();  // 帧数/时间 = 帧率
        frameCount = 0;                      // 重置帧数计数
        lastTime = currentTime;              // 更新时间戳
    }

    return fps;
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
    }

    return 0;
}

void Application::exit() {
    if(m_window){
        m_window->setShouldClose(true);
    }
}

void Application::render() {
    calcFrameRate();
    beginDrawScene();
    drawScene(0.0);
    endDrawScene();
}

void Application::calcFrameRate() {

}

void Application::beginDrawScene() {
    clearColor();
    return;
}