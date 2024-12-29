#pragma once
#include <string>
#include "Base/DXH.hpp"
#include "Base/GameTimer.hpp"
#include "App/Application.hpp"

using Microsoft::WRL::ComPtr;
class DX11Application : public Application{
public:
    DX11Application();
    virtual ~DX11Application();

    virtual bool init(const HINSTANCE, const WindowDesc param);

    int run(const int nShowCmd); 

    LRESULT msgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    void beginDrawScene();
    virtual void drawScene();
    void endDrawScene();

    virtual void updateScene(const float dt);
    
    virtual void onResize(const UINT msg, const WPARAM wParam, const LPARAM lParam);
    virtual void onMouseDown(WPARAM btnState, int x, int y);
    virtual void onMouseUp(WPARAM btnState, int x, int y);
    virtual void onMouseMove(WPARAM btnState, int x, int y);
    float aspectRatio() {
        return _attribute.winAttr.width * 1.0 / _attribute.winAttr.height;
    }

    HWND winId() {
        return _winId;
    }
private:
    void createMainWindow(const HINSTANCE instance);
    void initD3DEnv(const HWND);
    void calcFrameRate();
    void updateRenderTargetWhileResize();

protected:
    ComPtr<ID3D11Device> _pd3dDevice{};
    ComPtr<ID3D11DeviceContext> _pd3dDeviceCtx{  };
    ComPtr<IDXGISwapChain> _pd3dSwapChain{};
    ComPtr<ID3D11RenderTargetView> _pd3dRenderTargetView{};
    ComPtr< ID3D11Texture2D> _depthBuffer{};
    ComPtr<ID3D11DepthStencilView> _depthView{};
};