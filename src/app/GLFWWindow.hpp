#ifndef GLFW_WINDOW_H
#define GLFW_WINDOW_H

#include <string>
#include <functional>
#include "WindowProperties.hpp"

// 回调函数类型定义
using KeyCallback = std::function<void(int key, int scancode, int action, int mods)>;
using MouseButtonCallback = std::function<void(int button, int action, int mods)>;
using MouseMoveCallback = std::function<void(double xpos, double ypos)>;
using MouseScrollCallback = std::function<void(double xoffset, double yoffset)>;
using WindowResizeCallback = std::function<void(int width, int height)>;

struct GLFWwindow;
class GLFWWindow {
public:
    // 静态回调转发函数
    static void staticKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void staticMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void staticMouseMoveCallback(GLFWwindow* window, double xpos, double ypos);
    static void staticMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void staticWindowResizeCallback(GLFWwindow* window, int width, int height);

public:
    GLFWWindow(const GLFWWindowProperties& properties = GLFWWindowProperties());
    ~GLFWWindow();

    // 窗口生命周期管理
    bool initialize();
    void shutdown();

    // 事件和帧管理
    void pollEvents();
    void beginFrame();
    void endFrame();

    // 窗口状态
    bool shouldClose() const;
    void setShouldClose(bool value);

    // 属性访问
    GLFWWindowProperties getProperties() const { return m_properties; }
    void setProperties(const GLFWWindowProperties& properties) { m_properties = properties; }

    // 属性设置接口
    void setTitle(const std::string& title);
    void setSize(unsigned int width, unsigned int height);
    void setPosition(int x, int y);
    void setVsync(bool enabled);
    void setFullscreen(bool enabled);

    // 输入状态查询
    bool isKeyPressed(int key) const;
    bool isMouseButtonPressed(int button) const;
    void getMousePosition(double& x, double& y) const;
    void getMouseDelta(double& dx, double& dy) const;

    // 回调设置接口
    void setKeyCallback(const KeyCallback& callback) { m_keyCallback = callback; }
    void setMouseButtonCallback(const MouseButtonCallback& callback) { m_mouseButtonCallback = callback; }
    void setMouseMoveCallback(const MouseMoveCallback& callback) { m_mouseMoveCallback = callback; }
    void setMouseScrollCallback(const MouseScrollCallback& callback) { m_mouseScrollCallback = callback; }
    void setWindowResizeCallback(const WindowResizeCallback& callback) { m_windowResizeCallback = callback; }

    // ImGui相关
    bool initializeImGui();
    void shutdownImGui();
    void renderImGui();

    // 获取原生窗口指针
    GLFWwindow* getNativeGLFWWindow() const { return m_window; }
private:
    void newImGuiFrame();

private:
    GLFWwindow* m_window;
    GLFWWindowProperties m_properties;
    bool m_initialized;
    bool m_imguiInitialized;

    // 鼠标状态
    double m_mouseX;
    double m_mouseY;
    double m_lastMouseX;
    double m_lastMouseY;
    double m_mouseDeltaX;
    double m_mouseDeltaY;
    bool m_firstMouse;

    // 回调函数存储
    KeyCallback m_keyCallback;
    MouseButtonCallback m_mouseButtonCallback;
    MouseMoveCallback m_mouseMoveCallback;
    MouseScrollCallback m_mouseScrollCallback;
    WindowResizeCallback m_windowResizeCallback;
};

#endif // GLFW_WINDOW_H
    