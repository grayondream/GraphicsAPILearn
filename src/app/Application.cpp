#include "Application.hpp"
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

Application::Application() {
    LOGI("Game Start!");
}

Application::~Application() {
    if (_pd3dDeviceCtx) {
        _pd3dDeviceCtx->ClearState();
    }

    LOGI("Game End!");
}

void Application::initD3DEnv(const HWND winId) {
    std::tie(_pd3dDevice, _pd3dDeviceCtx) = CreateD3DContextAndDevice();
    const auto quality = GetD3DMSAAQuality(_pd3dDevice);
    _pd3dSwapChain = CreateD3DSwapChain(_pd3dDevice, winId, quality, _attribute.winAttr.width, _attribute.winAttr.height, _attribute.enableMssa);
    updateRenderTargetWhileResize();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Application *pApp{};
     // 获取窗口实例
    pApp = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (pApp) {
        // 调用实例的成员函数
        return pApp->msgProc(hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void Application::createMainWindow(const HINSTANCE hInstance){
    const char CLASS_NAME[] = "Sample Window Class";
    // 注册窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    // 创建窗口
    _winId = CreateWindowEx(
        0,
        CLASS_NAME,
        _attribute.winAttr.title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        _attribute.winAttr.width, _attribute.winAttr.height,
        NULL, NULL, hInstance, NULL
    );

    eh::ExitIfFailed(_winId, "Failed to create window, the window handle is nullptr");
    SetWindowLongPtr(_winId, GWLP_USERDATA, (LONG_PTR)this);
}

bool Application::init(const HINSTANCE hInstance, const CreateParam param){
    _attribute = param;
    createMainWindow(hInstance);
    initD3DEnv(_winId);
    LOGI("Initialize D3D environment successed");
    return true;
}

int Application::run(const int nShowCmd){
    ShowWindow(_winId, nShowCmd);
    MSG msg{};
    _timer.reset();
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }else {
            _timer.tick();
            if (!_state.paused) {
                calcFrameRate();
                updateScene(_timer.deltaTime());
                drawScene();
            }else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }

    return 0;
}

void Application::calcFrameRate() {
    static int frameCount = 0;
    static float timePassed = 0.0f;
    frameCount++;
    if (_timer.totalTime() - timePassed < 1.0f) {
        return;
    }

    const float fps = frameCount;
    const float mspf = 1000.f / fps;
    std::wostringstream os;
    os.precision(8);
    os << _attribute.winAttr.title.c_str() << L""
        << L"FPS: " << fps << L" "
        << L"Frame Time: " << mspf << L"(ms)";
    SetWindowTextW(_winId, os.str().c_str());
    frameCount = 0;
    timePassed += 1.0f;
}

void Application::updateRenderTargetWhileResize() {
    _pd3dRenderTargetView = nullptr;
    _depthBuffer = nullptr;
    _depthView = nullptr;
    LOGI("Resize Render Target into [{},{}]", _attribute.winAttr.width, _attribute.winAttr.height);
    eh::ExitIfFailed(_pd3dSwapChain->ResizeBuffers(1, _attribute.winAttr.width, _attribute.winAttr.height, DXGI_FORMAT_R8G8B8A8_UNORM, 0), "Failed to Resize Buffer");
    
    ComPtr<ID3D11Texture2D> pBackBuffer{};
    _pd3dSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(pBackBuffer.GetAddressOf()));
    auto hr = _pd3dDevice->CreateRenderTargetView(pBackBuffer.Get(), 0, _pd3dRenderTargetView.GetAddressOf());
    eh::ExitIfFailed(hr, "Failed to create render target view!");
    
    const auto quality = GetD3DMSAAQuality(_pd3dDevice);
    std::tie(_depthBuffer, _depthView) = D3DCreateRenderTexture(_pd3dDevice, quality, _attribute.winAttr.width, _attribute.winAttr.height, _attribute.enableMssa);
    _pd3dDeviceCtx->OMSetRenderTargets(1, _pd3dRenderTargetView.GetAddressOf(), _depthView.Get());
    D3DSetupViewPort(_pd3dDeviceCtx, _attribute.winAttr.width, _attribute.winAttr.height);
}

void Application::onResize(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
    if (LOWORD(lParam) != 0 && HIWORD(lParam) != 0) {
        _attribute.winAttr.width = LOWORD(lParam);
        _attribute.winAttr.height = HIWORD(lParam);
    }
    
    assert(_pd3dDevice && _pd3dDeviceCtx);
    switch (wParam) {
    case SIZE_MINIMIZED: {
        _state.paused = true;
        _state.minimized = true;
        _state.maximized = false;
    }break;
    case SIZE_MAXIMIZED: {
        _state.paused = false;
        _state.minimized = false;
        _state.maximized = true;
    }break;
    case SIZE_RESTORED: {
        if (_state.minimized) {
            _state.paused = false;
            _state.minimized = false;
        }
        else if (_state.maximized) {
            _state.paused = false;
            _state.maximized = false;
        }
        else if (_state.resizing) {
            //TODO:
        }
    }break;
    case WM_ENTERSIZEMOVE: {
        _state.paused = true;
        _state.resizing = true;
        _timer.stop();
        return;
    }break;
    case WM_EXITSIZEMOVE: {
        _state.paused = false;
        _state.resizing = false;
        _timer.start();
    }break;
    }

    updateRenderTargetWhileResize();
}

void Application::updateScene(const float dt) {
}

void Application::drawScene() {
    _pd3dDeviceCtx->ClearRenderTargetView(_pd3dRenderTargetView.Get(), reinterpret_cast<const float*>(&Colors::Red));
    _pd3dDeviceCtx->ClearDepthStencilView(_depthView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    eh::ExitIfFailed(_pd3dSwapChain->Present(0, 0), "Persent Failed");
}

void Application::onMouseDown(WPARAM btnState, int x, int y) { }
void Application::onMouseUp(WPARAM btnState, int x, int y) { }
void Application::onMouseMove(WPARAM btnState, int x, int y) { }

LRESULT Application::msgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_ACTIVATE: {
        if (LOWORD(wParam) == WA_INACTIVE) {
            _state.paused = true;
            _timer.stop();
        }
        else {
            _state.paused = false;
            _timer.start();
        }
        return 0;
        }break;
    case WM_SIZE:
        onResize(msg, wParam, lParam);
        return 0;
    case WM_ENTERSIZEMOVE: {
        _state.paused = true;
        _state.resizing = true;
        _timer.stop();
        return 0;
    } break;
    case WM_EXITSIZEMOVE:
        _state.paused = false;
        _state.resizing = false;
        _timer.start();
        onResize(msg, wParam, lParam);
        return 0;
    case WM_MENUCHAR:
        // Don't beep when we alt-enter.
        return MAKELRESULT(0, MNC_CLOSE);

        // Catch this message so to prevent the window from becoming too small.
    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
        ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
        return 0;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        onMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        onMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        onMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}