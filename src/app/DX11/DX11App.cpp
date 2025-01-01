#include "DX11App.hpp"
#include <iostream>
#include <format>
#include <cassert>
#include <sstream>
#include <chrono>
#include <thread>

#include "Base/DXBaseConexpr.hpp"
#include "EH/ErrorHandle.hpp"
#include "Base/GameTimer.hpp"
#include "Base/Log.hpp"
#include "Base/Utils.hpp"
#include <windowsx.h>

namespace eh = ErrorHandle;
using namespace base::log;
using namespace Utils;

DX11App::DX11App() {
}

DX11App::~DX11App() {
    if (_pd3dSwapChain) {
        _pd3dSwapChain->SetFullscreenState(FALSE, 0);
    }

    if (_pd3dDeviceCtx) {
        _pd3dDeviceCtx->ClearState();
    }

    LOGI("Game End!");
}

void DX11App::initD3DEnv(const HWND winId) {
    std::tie(_pd3dDevice, _pd3dDeviceCtx, _pd3dSwapChain) = CreateD3DDeviceAndtSwapChain(winId, _attribute.winAttr.width, _attribute.winAttr.height);
    updateRenderTargetWhileResize();
}

bool DX11App::init(const HINSTANCE hInstance, const WindowDesc& param){
    Application::init(hInstance, param);
    initD3DEnv(winId());
    LOGI("Initialize D3D environment successed");
    return true;
}

void DX11App::updateRenderTargetWhileResize() {
    _pd3dRenderTargetView = nullptr;
    _depthBuffer = nullptr;
    _depthView = nullptr;
    LOGI("Resize Render Target into [{},{}]", _attribute.winAttr.width, _attribute.winAttr.height);
    //eh::ExitIfFailed(_pd3dSwapChain->ResizeBuffers(1, _attribute.winAttr.width, _attribute.winAttr.height, DXGI_FORMAT_R8G8B8A8_UNORM, 0), "Failed to Resize Buffer");
    
    ComPtr<ID3D11Texture2D> pBackBuffer{};
    _pd3dSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(pBackBuffer.GetAddressOf()));
    auto hr = _pd3dDevice->CreateRenderTargetView(pBackBuffer.Get(), 0, _pd3dRenderTargetView.GetAddressOf());
    eh::ExitIfFailed(hr, "Failed to create render target view!");
    
    const auto quality = GetD3DMSAAQuality(_pd3dDevice);
    //std::tie(_depthBuffer, _depthView) = D3DCreateRenderTexture(_pd3dDevice, quality, _attribute.winAttr.width, _attribute.winAttr.height, _attribute.enableMssa);
    _pd3dDeviceCtx->OMSetRenderTargets(1, _pd3dRenderTargetView.GetAddressOf(), NULL);
    D3DSetupViewPort(_pd3dDeviceCtx, _attribute.winAttr.width, _attribute.winAttr.height);
}

void DX11App::updateScene(const float dt) {
}

void DX11App::clearColor() {
    _pd3dDeviceCtx->ClearRenderTargetView(_pd3dRenderTargetView.Get(), reinterpret_cast<const float*>(&Colors::LightSteelBlue));
}

void DX11App::beginDrawScene() {
    clearColor();
}

void DX11App::drawScene() {

}

void DX11App::endDrawScene() {
    eh::ExitIfFailed(_pd3dSwapChain->Present(0, 0), "Persent Failed");
}
