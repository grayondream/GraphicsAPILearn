#pragma once
#include <string>
#include "IApplication.hpp"
#include "GLFWWindow.hpp"
#include <memory>

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

    virtual bool init(const GLFWWindowProperties& properties) override;

    virtual int run() override; 

    void exit();

protected:
    void render();
    virtual void clearColor() {}
    virtual void beginDrawScene();
    virtual void drawScene(const float dt){}
    virtual void endDrawScene(){}

private:
    void calcFrameRate();
    void updateFrameRate();
    
protected:
    std::unique_ptr<GLFWWindow> m_window{};
    State m_state{};
};