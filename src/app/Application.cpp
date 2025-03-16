#include <iostream>

#include <cassert>
#include <sstream>
#include <chrono>
#include <thread>

#include "Application.hpp"
#include "EH/ErrorHandle.hpp"
#include "Base/GameTimer.hpp"
#include "Base/Log.hpp"
#include <windowsx.h>
#include <imgui.h>
#include <backends/imgui_impl_win32.h>

namespace eh = ErrorHandle;

Application::Application() {
    
}

Application::~Application() {
    if (_uiInitialized) {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
}

LRESULT CALLBACK AppWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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
    wc.lpfnWndProc = AppWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    RegisterClass(&wc);

    // 创建窗口
    _winId = CreateWindowEx(
        0,
        CLASS_NAME,
        _attribute.winAttr.title.c_str(),
        WS_OVERLAPPEDWINDOW,
        400, 400,
        _attribute.winAttr.width, _attribute.winAttr.height,
        NULL, NULL, hInstance, NULL
    );

    eh::ExitIfFailed(_winId, "Failed to create window, the window handle is nullptr");
    SetWindowLongPtr(_winId, GWLP_USERDATA, (LONG_PTR)this);
}

bool Application::init(const HINSTANCE hInstance, const WindowDesc& param){
    _attribute = param;
    createMainWindow(hInstance);
    LOGI("Create Main Windows successed");
    return true;
}

int Application::run(const int nShowCmd){
    ShowWindow(_winId, nShowCmd);
    _running = true;
    MSG msg{};
    _timer.reset();
    while (_running && GetMessage(&msg, 0, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        render();
    }

    return 0;
}

void Application::exit() {
    _running = false;
}

void Application::render() {
    if (_state.paused) {
        return;
    }

    _timer.tick();
    calcFrameRate();
    beginDrawScene();
    drawScene(_timer.deltaTime());
    endDrawScene();
}

void Application::initImGUI() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    ImGui_ImplWin32_Init(winId());
    _uiInitialized = true;
}

void Application::calcFrameRate() {
    static int frameCount = 0;
    static float timePassed = 0.0f;
    frameCount++;
    const auto gameTime = _timer.totalTime();
    if (gameTime - timePassed < 1.0f) {
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

void Application::onResize(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
    if (LOWORD(lParam) != 0 && HIWORD(lParam) != 0) {
        _attribute.winAttr.width = LOWORD(lParam);
        _attribute.winAttr.height = HIWORD(lParam);
    }
    
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

    //updateRenderTargetWhileResize();
}

void Application::beginDrawScene() {
    clearColor();
    return;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT Application::msgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_PAINT:
        // 触发重绘
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT menu
            return 0;
        break;
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
        onMouseDown(msg, wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        onMouseUp(msg, wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        onMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR:
        onKeyBoardEvent(msg, wParam, lParam); break;
    case WM_MOUSEWHEEL:
        onMouseScroll(msg, wParam, lParam);break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Application::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
    switch (msg) {
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            _running = false;
            std::cout << "ESC key pressed!" << std::endl;
            PostQuitMessage(0);  // 退出程序
        }
        break;
    }
    }
}