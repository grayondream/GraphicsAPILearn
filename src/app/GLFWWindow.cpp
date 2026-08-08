#include "GLFWWindow.hpp"
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "base/Log.hpp"

// 静态回调函数实现
void GLFWWindow::staticKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (!window) {
        return;
    }

    // 通过用户指针获取GLFWWindow实例
    GLFWWindow* instance = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
    if (instance && instance->m_keyCallback) {
        instance->m_keyCallback(key, scancode, action, mods);
    }
}

void GLFWWindow::staticMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (!window) {
        return;

    }

    GLFWWindow* instance = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
    if (instance && instance->m_mouseButtonCallback) {
        instance->m_mouseButtonCallback(button, action, mods);
    }
}

void GLFWWindow::staticMouseMoveCallback(GLFWwindow* window, double xpos, double ypos) {
    if (!window) {
        return;
    }

    GLFWWindow* instance = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
    if (!instance) {
        return;
    }
    
    if (instance->m_mouseMoveCallback) {
        instance->m_mouseMoveCallback(xpos, ypos);
    }
}

void GLFWWindow::staticMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (!window) {
        return;
    }

    GLFWWindow* instance = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
    if (instance && instance->m_mouseScrollCallback) {
        instance->m_mouseScrollCallback(xoffset, yoffset);
    }
}

void GLFWWindow::staticWindowResizeCallback(GLFWwindow* window, int width, int height) {
    if (!window) {
        return;
    }

    GLFWWindow* instance = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
    if (instance) {
        if (instance->m_windowResizeCallback) {
            instance->m_windowResizeCallback(width, height);
        }
    }
}

// 构造函数
GLFWWindow::GLFWWindow(const GLFWWindowProperties& properties)
    : m_properties(properties), m_window(nullptr), m_initialized(false) {}

// 析构函数
GLFWWindow::~GLFWWindow() {
    shutdown();
}

GLFWWindow& GLFWWindow::setUserPointer(void* pointer) {
    m_userPointer = pointer;
    return *this;
}

void* GLFWWindow::getUserPointer() const {
    return m_userPointer;
}

// 初始化窗口
bool GLFWWindow::initialize() {
    if (m_initialized) {
        std::cout << "Window already initialized" << std::endl;
        return true;
    }

    if (!glfwInit()) {
        return false;
    }

        // 配置GLFW窗口 hints（不指定图形API，保持通用）
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    // 创建窗口
    m_window = glfwCreateWindow(
        m_properties.width, m_properties.height,
        m_properties.title.c_str(),
        m_properties.fullscreen ? glfwGetPrimaryMonitor() : nullptr,
        nullptr
    );

    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        return false;
    }

    if (m_properties.vsync) {
        glfwSwapInterval(1);
    }
    else {
        glfwSwapInterval(0);
    }

    // 设置用户指针，用于回调中获取实例
    glfwSetWindowUserPointer(m_window, this);

    // 设置窗口位置
    glfwSetWindowPos(m_window, m_properties.xPos, m_properties.yPos);

    // 注册回调函数
    glfwSetKeyCallback(m_window, staticKeyCallback);
    glfwSetMouseButtonCallback(m_window, staticMouseButtonCallback);
    glfwSetCursorPosCallback(m_window, staticMouseMoveCallback);
    glfwSetScrollCallback(m_window, staticMouseScrollCallback);
    glfwSetWindowSizeCallback(m_window, staticWindowResizeCallback);
    
    m_initialized = true;
    return true;
}

bool GLFWWindow::initGLContext() {
    if (!m_initialized) {
        return true;
    }

    glfwMakeContextCurrent(m_window);
    if (glfwGetCurrentContext() == nullptr) {
        LOGE("Failed to make context current");
        shutdown();
        return false;
    }

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOGE("Failed to initialize GLAD");
        return false;
    }

    return true;
}

GLFWWindow& GLFWWindow::setMouseMoveCallback(const MouseMoveCallback& callback) {
    m_mouseMoveCallback = callback;
    return *this;
}

GLFWWindow& GLFWWindow::setMouseScrollCallback(const MouseScrollCallback& callback) {
    m_mouseScrollCallback = callback;
    return *this;
}

GLFWWindow& GLFWWindow::setMouseButtonCallback(const MouseButtonCallback& callback) {
    m_mouseButtonCallback = callback;
    return *this;
}

GLFWWindow& GLFWWindow::setWindowResizeCallback(const WindowResizeCallback& callback) {
    m_windowResizeCallback = callback;
    return *this;
}


GLFWWindow& GLFWWindow::setKeyCallback(const KeyCallback& callback) {
    m_keyCallback = callback;
    return *this;
}

// 关闭窗口
GLFWWindow& GLFWWindow::shutdown() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    m_initialized = false;
    return *this;
}

GLFWWindow& GLFWWindow::updateFrameRate(float fs){
    const auto title = m_properties.title;
    const auto newTitle = title + " - " + std::to_string(fs) + " FPS";
    glfwSetWindowTitle(m_window, newTitle.c_str());
    return *this;
}

GLFWWindow& GLFWWindow::swapBuffers() {
    if (!m_initialized) {
        return *this;
    }
    glfwSwapBuffers(m_window);
    return *this;
}


// 轮询事件
GLFWWindow& GLFWWindow::pollEvents() {
    if (!m_initialized) {
        return *this;
    }
    glfwPollEvents();
    return *this;
}

// 开始帧
GLFWWindow& GLFWWindow::beginFrame() {
    return *this;
}

// 结束帧（交换缓冲区）
GLFWWindow& GLFWWindow::endFrame() {
    // 由具体图形API实现缓冲区交换
    return *this;
}

// 窗口是否应该关闭
bool GLFWWindow::shouldClose() const {
    return m_initialized ? glfwWindowShouldClose(m_window) : true;
}

// 设置窗口关闭状态
GLFWWindow& GLFWWindow::setShouldClose(bool value) {
    if (!m_initialized) {
        return *this;
    }

    glfwSetWindowShouldClose(m_window, value ? GLFW_TRUE : GLFW_FALSE);
    return *this;
}

// 设置窗口标题
GLFWWindow& GLFWWindow::setTitle(const std::string& title) {
    if (m_initialized && !title.empty()) {
        m_properties.title = title;
        glfwSetWindowTitle(m_window, title.c_str());
    }

    return *this;
}

// 设置窗口大小
GLFWWindow& GLFWWindow::setSize(unsigned int width, unsigned int height) {
    if (m_initialized && width > 0 && height > 0) {
        m_properties.width = width;
        m_properties.height = height;
        glfwSetWindowSize(m_window, width, height);
    }

    return *this;
}

// 设置窗口位置
GLFWWindow& GLFWWindow::setPosition(int x, int y) {
    if (m_initialized) {
        m_properties.xPos = x;
        m_properties.yPos = y;
        glfwSetWindowPos(m_window, x, y);
    }

    return *this;
}

// 设置垂直同步
GLFWWindow& GLFWWindow::setVsync(bool enabled) {
    if (m_initialized) {
        m_properties.vsync = enabled;
        // 实际VSync控制由具体图形API实现
    }

    return *this;
}

// 设置全屏模式
GLFWWindow& GLFWWindow::setFullscreen(bool enabled) {
    if (!m_initialized) {
        return *this;
    }

    //TODO:
    return *this;
}

// 检查按键是否按下
bool GLFWWindow::isKeyPressed(int key) const {
    return m_initialized ? (glfwGetKey(m_window, key) == GLFW_PRESS) : false;
}

// 检查鼠标按钮是否按下
bool GLFWWindow::isMouseButtonPressed(int button) const {
    return m_initialized ? (glfwGetMouseButton(m_window, button) == GLFW_PRESS) : false;
}

// 获取鼠标位置
void GLFWWindow::getMousePosition(double& x, double& y) const {
    if (m_initialized) {
        glfwGetCursorPos(m_window, &x, &y);
    } else {
        x = 0.0;
        y = 0.0;
    }
}
    