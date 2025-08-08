#pragma once
#include "App/ImGuiWindow.hpp"

struct GLFWwindow;
class ImGuiOpenglWindow : public ImGuiWindow{
public:
    ~ImGuiOpenglWindow();

public:
    virtual void init(GLFWwindow*win) override;
    virtual void newFrame() override;
    virtual void render() override;
    virtual void destroy() override;
};
