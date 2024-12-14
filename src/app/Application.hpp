#pragma once
#include <string>
#include "Base/DXH.hpp"
#include "Base/GameTimer.hpp"

using Microsoft::WRL::ComPtr;
class Application{
public:
    struct WindowsAttribute {
        int width;
        int height;
        std::string title{};
    };
    struct CreateParam {
    public:
        WindowsAttribute winAttr;
        bool enableMssa;
    };

    struct State {
        bool paused{};
    };
public:
    Application();
    ~Application();

    bool init(const HINSTANCE, const CreateParam param);

    int run(const int nShowCmd); 

    LRESULT msgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void onResize();

    void updateScene(const float dt);

    void drawScene();

    void onMouseDown(WPARAM btnState, int x, int y);
    void onMouseUp(WPARAM btnState, int x, int y);
    void onMouseMove(WPARAM btnState, int x, int y);

    void createMainWindow(const HINSTANCE instance);
    void initD3DEnv(const HWND);
    void calcFrameRate();

private:
    CreateParam _attribute{};
    State _state{};
    HWND _winId{};
    GameTimer _timer{};
    ComPtr<ID3D11Device> _pd3dDevice{};
    ComPtr<ID3D11DeviceContext> _pd3dDeviceCtx{  };
    ComPtr<IDXGISwapChain> _pd3dSwapChain{};
    ComPtr<ID3D11RenderTargetView> _pd3dRenderTargetView{};

};