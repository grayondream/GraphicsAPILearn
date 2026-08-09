#pragma once
#include <string>
#include "IApplication.hpp"
#include "GLFWWindow.hpp"
#include <memory>
#include "IImGuiWindow.hpp"

class Application : public IApplication{
public:
    struct State {
        bool paused{};
        bool minimized{};
        bool maximized{};
        bool resizing{};
    };
public:
    Application();
    virtual ~Application();

    virtual bool init(const GLFWWindowProperties& properties) override final;

    virtual int run() override final; 

    void exit() override final;

public:
    virtual void onMouseMove(double x, double y);
    virtual void onMouseScroll(double xoffset, double yoffset);
    virtual void onMouseButton(int button, int action, int mods);
    virtual void onKeyPress(int key, int scancode, int action, int mods);
    virtual void onWindowResize(int width, int height);
    
protected:
    virtual bool initGraphics() { return true; }
    virtual bool initApp() { return true; }
    virtual void renderBeforeLoop(){}
    virtual void render();
    virtual void clearColor() {}
    virtual void beginDrawScene();
    virtual void drawScene(const float dt){}
    virtual void endDrawScene(){}
    float aspectRatio() const;

private:
    void updateFrameRate();
    void initInputEvent();
    
protected:
    std::unique_ptr<GLFWWindow> m_window{};
    std::unique_ptr<IImGuiWindow> m_imguiWindow{};
};