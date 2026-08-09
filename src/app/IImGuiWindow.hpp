#pragma once
struct GLFWwindow;
class IImGuiWindow{
public:
    virtual ~IImGuiWindow() = default;

public:
    virtual void init(GLFWwindow* win);
    virtual void newFrame();
    virtual void render();
    virtual void destroy();
};
