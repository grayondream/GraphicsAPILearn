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

    GLFWWindow& shutdown();
    bool initGLContext();
    
    // 事件和帧管理
    GLFWWindow& pollEvents();
    GLFWWindow& swapBuffers();

    GLFWWindow& beginFrame();
    GLFWWindow& endFrame();

    // 窗口状态
    bool shouldClose() const;
    GLFWWindow& setShouldClose(bool value);

    // 属性访问
    GLFWWindowProperties getProperties() const { return m_properties; }
    void setProperties(const GLFWWindowProperties& properties) { m_properties = properties; }

    // 属性设置接口
    GLFWWindow& setTitle(const std::string& title);
    GLFWWindow& setSize(unsigned int width, unsigned int height);
    GLFWWindow& setPosition(int x, int y);
    GLFWWindow& setVsync(bool enabled);
    GLFWWindow& setFullscreen(bool enabled);
    GLFWWindow& updateFrameRate(float fs);
    GLFWWindow& setUserPointer(void* pointer);
    void* getUserPointer() const;

    // 输入状态查询
    bool isKeyPressed(int key) const;
    bool isMouseButtonPressed(int button) const;
    void getMousePosition(double& x, double& y) const;

    // 回调设置接口
    GLFWWindow& setKeyCallback(const KeyCallback& callback);
    GLFWWindow& setMouseButtonCallback(const MouseButtonCallback& callback);
    GLFWWindow& setMouseMoveCallback(const MouseMoveCallback& callback);
    GLFWWindow& setMouseScrollCallback(const MouseScrollCallback& callback);
    GLFWWindow& setWindowResizeCallback(const WindowResizeCallback& callback);

    // 获取原生窗口指针
    GLFWwindow* getNativeGLFWWindow() const { return m_window; }

private:
    GLFWwindow* m_window;
    void* m_userPointer;

    GLFWWindowProperties m_properties;
    bool m_initialized;
    
    // 回调函数存储
    KeyCallback m_keyCallback;
    MouseButtonCallback m_mouseButtonCallback;
    MouseMoveCallback m_mouseMoveCallback;
    MouseScrollCallback m_mouseScrollCallback;
    WindowResizeCallback m_windowResizeCallback;
};

#endif // GLFW_WINDOW_H
    