#pragma once
struct GLFWwindow;
class ImGuiWindow{
public:
    virtual ~ImGuiWindow() = default;

public:
    virtual void init(GLFWwindow* win);
    virtual void newFrame();
    virtual void render();
    virtual void destroy();
};
