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
        bool minimized{};
        bool maximized{};
        bool resizing{};
    };
public:
    Application();
    ~Application();

    bool init(const HINSTANCE, const CreateParam param);

    int run(const int nShowCmd); 

    LRESULT msgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    

    void updateScene(const float dt);

    void drawScene();
    
    void onResize(const UINT msg, const WPARAM wParam, const LPARAM lParam);
    void onMouseDown(WPARAM btnState, int x, int y);
    void onMouseUp(WPARAM btnState, int x, int y);
    void onMouseMove(WPARAM btnState, int x, int y);

    void createMainWindow(const HINSTANCE instance);
    void initD3DEnv(const HWND);
    void calcFrameRate();
    void updateRenderTargetWhileResize();
private:
    CreateParam _attribute{};
    State _state{};
    HWND _winId{};
    GameTimer _timer{};
    ComPtr<ID3D11Device> _pd3dDevice{};
    ComPtr<ID3D11DeviceContext> _pd3dDeviceCtx{  };
    ComPtr<IDXGISwapChain> _pd3dSwapChain{};
    ComPtr<ID3D11RenderTargetView> _pd3dRenderTargetView{};
    ComPtr< ID3D11Texture2D> _depthBuffer{};
    ComPtr<ID3D11DepthStencilView> _depthView{};

};