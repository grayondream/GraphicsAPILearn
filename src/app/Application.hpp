#pragma once
#include <string>
#include "Base/GameTimer.hpp"
#include "IApplication.hpp"

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

    virtual bool init(const HINSTANCE, const WindowDesc& param) override;

    virtual int run(const int nShowCmd) override; 

    LRESULT msgProc(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam);

    void exit();

protected:
    void render();
    virtual void clearColor() {}
    virtual void beginDrawScene();
    virtual void drawScene(const float dt);
    virtual void endDrawScene();
    
    virtual void onResize(const UINT msg, const WPARAM wParam, const LPARAM lParam);
    virtual void onMouseDown(WPARAM btnState, int x, int y);
    virtual void onMouseUp(WPARAM btnState, int x, int y);
    virtual void onMouseMove(WPARAM btnState, int x, int y);
    virtual void onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam);
    virtual void onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam);
    float aspectRatio() {
        return _attribute.winAttr.width * 1.0 / _attribute.winAttr.height;
    }

    HWND winId() {
        return _winId;
    }

    void initImGUI();

private:
    void createMainWindow(const HINSTANCE instance);
    void calcFrameRate();
    

protected:
    WindowDesc _attribute{};
    State _state{};
    HWND _winId{};
    GameTimer _timer{};
    bool _uiInitialized{ false };
    bool _running{ false };
};