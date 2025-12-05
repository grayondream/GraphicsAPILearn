#include "GLApp.hpp"
#include "Base/ErrorHandle.hpp"
#include "Base/Log.hpp"
#include "GL/GLHeader.hpp"
#include "ImGuiOpenglWindow.hpp"
#include <imgui.h>

using namespace ErrorHandle;

GLApp::GLApp() {

}

GLApp::~GLApp() {

}

bool GLApp::initGraphics() {
    // 设置OpenGL上下文
    if (!m_window->initGLContext()) {
        LOGE("Failed to initialize GL Context");
        return false;
    }
   
    // 打印OpenGL版本信息
    LOGI("OpenGL Vendor: {}", (char*)glGetString(GL_VENDOR));
    LOGI("OpenGL Renderer: {}", (char*)glGetString(GL_RENDERER));
    LOGI("OpenGL Version: {}", (char*)glGetString(GL_VERSION));
    
    // 设置视口
    int width, height;
    auto props = m_window->getProperties();
    width = props.width;
    height = props.height;
    glViewport(0, 0, width, height);
    // 启用深度测试
    glEnable(GL_DEPTH_TEST);
    m_imguiWindow = std::make_unique<ImGuiOpenglWindow>();
    m_imguiWindow->init(m_window->getNativeGLFWWindow());
    return true;
}

void GLApp::clearColor() {
    glClearColor(0.1, 0.1, 0.1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void GLApp::beginDrawScene() {
    return Application::beginDrawScene();
}

void GLApp::drawScene(const float dt) {
    ImGui::Begin("OpenGL");
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Hello Graphic! %.1f FPS", io.Framerate);  // Display current FPS
    ImGui::End();

    return Application::drawScene(dt);
}

void GLApp::endDrawScene() {
    m_window->swapBuffers();
    return Application::endDrawScene();
}
