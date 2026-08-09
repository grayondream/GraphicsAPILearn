#pragma once
#include "app/IImGuiWindow.hpp"

struct GLFWwindow;
class ImGuiOpenglWindow : public IImGuiWindow{
public:
    ~ImGuiOpenglWindow();

public:
    virtual void init(GLFWwindow*win) override;
    virtual void newFrame() override;
    virtual void render() override;
    virtual void destroy() override;
};
