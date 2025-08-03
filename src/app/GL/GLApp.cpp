#include "GLApp.hpp"
#include "EH/ErrorHandle.hpp"
#include "Base/Log.hpp"
#include "glad/glad.h"
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_win32.h>
using namespace ErrorHandle;

GLApp::GLApp() {
    _glContext = nullptr;
}

GLApp::~GLApp() {
    if (_hdc) {
        ReleaseDC(winId(), _hdc);
    }

    if (_glContext) {
        ImGui_ImplOpenGL3_Shutdown();
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(_glContext);
        _glContext = nullptr;
    }
}

bool GLApp::init(const HINSTANCE hinstance, const WindowDesc& param) {
	Application::init(hinstance, param);
    _glContext = CreateOpenGLContext(winId());
    ExitIfFailed(!!_glContext, "Create GLContext Failed, the Gl Context is nullptr");
    ExitIfFailed(initGlad(), "Failed to Load OpenGL Glad!");
    char* version = (char*)glGetString(GL_VERSION);
    LOGI("OpenGL Version: {}", std::string(version));
    initImGUI();
    glEnable(GL_DEPTH_TEST);
    return true;
}

bool GLApp::initGlad() {
    return  !!gladLoadGL();
}

void GLApp::initImGUI() {
    Application::initImGUI();
    ImGui_ImplOpenGL3_Init();
}

HGLRC GLApp::CreateOpenGLContext(const HWND hwnd) {
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),  //  size of this pfd
        1,                     // version number
        PFD_DRAW_TO_WINDOW |   // support window
        PFD_SUPPORT_OPENGL |   // support OpenGL
        PFD_DOUBLEBUFFER,       // double buffered
        PFD_TYPE_RGBA,           // RGBA type
        32,                      // 32-bit color depth
        0, 0, 0, 0, 0, 0,        // color bits ignored
        0,                      // no alpha buffer
        0,                      // shift bit ignored
        0,                      // no accumulation buffer
        0, 0, 0, 0,             // accum bits ignored
        24,                     // 24-bit z-buffer (depth buffer)
        8,                      // 8-bit stencil buffer  <-- 关键：指定模板缓冲位数
        0,                      // no auxiliary buffer
        PFD_MAIN_PLANE,           // main layer
        0,                      // reserved
        0, 0, 0                 // layer masks ignored
    };

    _hdc = GetDC(hwnd);
    int pixelFormat = ChoosePixelFormat(_hdc, &pfd);
    SetPixelFormat(_hdc, pixelFormat, &pfd);

    // ���� OpenGL ������
    HGLRC hglrc = wglCreateContext(_hdc);
    wglMakeCurrent(_hdc, hglrc);
    return hglrc;
}

void GLApp::onResize(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
    Application::onResize(msg, wParam, lParam);
    if (wParam != SIZE_MINIMIZED) {
        glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
    }

    return;
}

void GLApp::clearColor() {
    glClearColor(0.1, 0.1, 0.1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void GLApp::beginDrawScene() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
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
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SwapBuffers(wglGetCurrentDC());
    return Application::endDrawScene();
}
