#include "GLApp.hpp"
#include "EH/ErrorHandle.hpp"
#include "Base/Log.hpp"
#include "glad/glad.h"
using namespace ErrorHandle;

GLApp::GLApp() {
}

GLApp::~GLApp() {
    if (_hdc) {
        ReleaseDC(winId(), _hdc);
    }

    if (_glContext) {
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
    glEnable(GL_DEPTH_TEST);
    return true;
}

bool GLApp::initGlad() {
    return  !!gladLoadGL();
}

HGLRC GLApp::CreateOpenGLContext(const HWND hwnd) {
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    _hdc = GetDC(hwnd);
    int pixelFormat = ChoosePixelFormat(_hdc, &pfd);
    SetPixelFormat(_hdc, pixelFormat, &pfd);

    // 创建 OpenGL 上下文
    HGLRC hglrc = wglCreateContext(_hdc);
    wglMakeCurrent(_hdc, hglrc);
    return hglrc;
}

void GLApp::clearColor() {
    glClearColor(173.0f / 255.0f, 216.0f / 255.0f, 230.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLApp::beginDrawScene() {
    Application::beginDrawScene();
}

void GLApp::drawScene() {
}

void GLApp::endDrawScene() {
    SwapBuffers(wglGetCurrentDC());
}

void GLApp::updateScene(const float dt) {
}
