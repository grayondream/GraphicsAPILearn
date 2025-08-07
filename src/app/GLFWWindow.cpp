#include "GLFWWindow.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include <iostream>
#include <glfw/glfw3.h>

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
    // 更新鼠标位置和 delta
    instance->m_lastMouseX = instance->m_mouseX;
    instance->m_lastMouseY = instance->m_mouseY;
    instance->m_mouseX = xpos;
    instance->m_mouseY = ypos;
    
    // 首次鼠标输入不计算 delta
    if (instance->m_firstMouse) {
        instance->m_lastMouseX = xpos;
        instance->m_lastMouseY = ypos;
        instance->m_firstMouse = false;
    }
    
    // 计算 delta (Y轴反转，符合常规图形应用习惯)
    instance->m_mouseDeltaX = xpos - instance->m_lastMouseX;
    instance->m_mouseDeltaY = instance->m_lastMouseY - ypos;
    
    // 调用用户回调
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
        // 更新内部尺寸记录
        instance->m_properties.width = width;
        instance->m_properties.height = height;
        
        // 调用用户回调
        if (instance->m_windowResizeCallback) {
            instance->m_windowResizeCallback(width, height);
        }
    }
}

// 构造函数
GLFWWindow::GLFWWindow(const GLFWWindowProperties& properties)
    : m_properties(properties), m_window(nullptr), m_initialized(false), m_imguiInitialized(false),
      m_firstMouse(true), m_mouseX(0), m_mouseY(0),
      m_lastMouseX(0), m_lastMouseY(0),
      m_mouseDeltaX(0), m_mouseDeltaY(0) {}

// 析构函数
GLFWWindow::~GLFWWindow() {
    shutdown();
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
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // 不创建默认上下文
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
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

    // 初始化鼠标状态
    m_firstMouse = true;
    glfwGetCursorPos(m_window, &m_mouseX, &m_mouseY);
    m_lastMouseX = m_mouseX;
    m_lastMouseY = m_mouseY;

    m_initialized = true;
    return true;
}

// 关闭窗口
void GLFWWindow::shutdown() {
    if (m_imguiInitialized) {
        shutdownImGui();
    }

    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    m_initialized = false;
}

// 轮询事件
void GLFWWindow::pollEvents() {
    if (!m_initialized) {
        return;
    }
    glfwPollEvents();
}

// 开始帧
void GLFWWindow::beginFrame() {
    // 重置鼠标 delta
    m_mouseDeltaX = 0.0;
    m_mouseDeltaY = 0.0;
    newImGuiFrame();
}

// 结束帧（交换缓冲区）
void GLFWWindow::endFrame() {
    // 由具体图形API实现缓冲区交换
}

// 窗口是否应该关闭
bool GLFWWindow::shouldClose() const {
    return m_initialized ? glfwWindowShouldClose(m_window) : true;
}

// 设置窗口关闭状态
void GLFWWindow::setShouldClose(bool value) {
    if (!m_initialized) {
        return;
    }

    glfwSetWindowShouldClose(m_window, value ? GLFW_TRUE : GLFW_FALSE);
}

// 设置窗口标题
void GLFWWindow::setTitle(const std::string& title) {
    if (m_initialized && !title.empty()) {
        m_properties.title = title;
        glfwSetWindowTitle(m_window, title.c_str());
    }
}

// 设置窗口大小
void GLFWWindow::setSize(unsigned int width, unsigned int height) {
    if (m_initialized && width > 0 && height > 0) {
        m_properties.width = width;
        m_properties.height = height;
        glfwSetWindowSize(m_window, width, height);
    }
}

// 设置窗口位置
void GLFWWindow::setPosition(int x, int y) {
    if (m_initialized) {
        m_properties.xPos = x;
        m_properties.yPos = y;
        glfwSetWindowPos(m_window, x, y);
    }
}

// 设置垂直同步
void GLFWWindow::setVsync(bool enabled) {
    if (m_initialized) {
        m_properties.vsync = enabled;
        // 实际VSync控制由具体图形API实现
    }
}

// 设置全屏模式
void GLFWWindow::setFullscreen(bool enabled) {
    if (!m_initialized) {
        return;
    }

    //TODO:
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

// 获取鼠标移动增量
void GLFWWindow::getMouseDelta(double& dx, double& dy) const {
    dx = m_mouseDeltaX;
    dy = m_mouseDeltaY;
}

// 初始化ImGui
bool GLFWWindow::initializeImGui() {
    if (m_imguiInitialized || !m_initialized) {
        return m_imguiInitialized;
    }

    // 初始化ImGui上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // 配置ImGui
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // 启用键盘导航
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // 启用游戏手柄导航

    // 设置ImGui样式
    ImGui::StyleColorsDark();

    // 初始化GLFW后端（不绑定特定渲染API）
    if (!ImGui_ImplGlfw_InitForOther(m_window, true)) {
        std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
        ImGui::DestroyContext();
        return false;
    }

    m_imguiInitialized = true;
    return true;
}

// 关闭ImGui
void GLFWWindow::shutdownImGui() {
    if (m_imguiInitialized) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_imguiInitialized = false;
    }
}

// 开始ImGui帧
void GLFWWindow::newImGuiFrame() {
    if (m_imguiInitialized) {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }
}

// 渲染ImGui
void GLFWWindow::renderImGui() {
    if (!m_imguiInitialized) {
        return;
    }

    ImGui::Render();
}
    