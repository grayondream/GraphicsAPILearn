#include "Application.hpp"
#include <iostream>
#include <format>
#include <cassert>
#include <sstream>
#include <chrono>
#include <thread>

#include "EH/ErrorHandle.hpp"
#include "Base/GameTimer.hpp"
#include "Base/Log.hpp"
#include "Base/Utils.hpp"

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
    _pd3dRenderTargetView = CreateD3DRenderTargetView(_pd3dSwapChain, _pd3dDevice);

    D3DSetupViewPort(_pd3dDeviceCtx, _attribute.winAttr.width, _attribute.winAttr.height);
    const auto [depthTexture, depthView] = D3DCreateRenderTexture(_pd3dDevice, quality, _attribute.winAttr.width, _attribute.winAttr.height, _attribute.enableMssa);
    _pd3dDeviceCtx->OMSetRenderTargets(1, _pd3dRenderTargetView.GetAddressOf(), depthView.Get());
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
    LOGI("Update Frame Info");
    frameCount = 0;
    timePassed += 1.0f;
}

void Application::onResize() {}

void Application::updateScene(const float dt) {}

void Application::drawScene() {}

void Application::onMouseDown(WPARAM btnState, int x, int y) { }
void Application::onMouseUp(WPARAM btnState, int x, int y) { }
void Application::onMouseMove(WPARAM btnState, int x, int y) { }

LRESULT Application::msgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
            EndPaint(hwnd, &ps);
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}