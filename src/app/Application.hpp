#pragma once
#include "Base/DXH.hpp"

using Microsoft::WRL::ComPtr;
class Application{
public:
    Application();
    ~Application();

    bool init(const HINSTANCE);

    int run(const int nShowCmd); 

    void onResize();

    void updateScene(const float dt);

    void drawScene();

    void onMouseDown();

    void onMouseUp();

    void onMouseMove();
private:
    void createMainWindow(const HINSTANCE instance);
    void initD3DEnv(const HWND);

private:
    HWND _winId{};
    ComPtr<ID3D11Device> _pd3dDevice{};
    ComPtr<ID3D11DeviceContext> _pd3dDeviceCtx{  };
    ComPtr<IDXGISwapChain> _pd3dSwapChain{};
    ComPtr<ID3D11RenderTargetView> _pd3dRenderTargetView{};

};