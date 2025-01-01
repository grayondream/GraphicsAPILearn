#pragma once
#include <string>
#include "Base/DXH.hpp"
#include "Base/GameTimer.hpp"
#include "App/Application.hpp"

using Microsoft::WRL::ComPtr;
class DX11App : public Application{
public:
    DX11App();
    virtual ~DX11App();

    virtual bool init(const HINSTANCE, const WindowDesc& param) override;

protected:
    virtual void clearColor() override;
    virtual void beginDrawScene();
    virtual void drawScene();
    virtual void endDrawScene();

    virtual void updateScene(const float dt);
    
private:
    void initD3DEnv(const HWND);
    void updateRenderTargetWhileResize();

protected:
    ComPtr<ID3D11Device> _pd3dDevice{};
    ComPtr<ID3D11DeviceContext> _pd3dDeviceCtx{  };
    ComPtr<IDXGISwapChain> _pd3dSwapChain{};
    ComPtr<ID3D11RenderTargetView> _pd3dRenderTargetView{};
    ComPtr< ID3D11Texture2D> _depthBuffer{};
    ComPtr<ID3D11DepthStencilView> _depthView{};
};