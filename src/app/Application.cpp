#include "Application.hpp"
#include <iostream>
#include <format>
#include <cassert>
#include "EH/ErrorHandle.hpp"
#include "Base/GameTimer.hpp"
#include "Base/Log.hpp"

#if defined(DEBUG) || defined(_DEBUG)
#define DEFAULT_DX_DEVICE_FLAG D3D11_CREATE_DEVICE_DEBUG
#else
#define DEFAULT_DX_DEVICE_FLAG 0
#endif

static inline constexpr int GAME_WIN_WIDTH = 720;
static inline constexpr int GAME_WIN_HEIGHT = 480;
static inline constexpr int GAME_ENABLE_MSAA = true;
namespace eh = ErrorHandle;
using namespace base::log;

inline static std::pair<ComPtr<ID3D11Device>, ComPtr<ID3D11DeviceContext>> CreateD3DContextAndDevice() {
    ComPtr<ID3D11Device> pDevice{};
    ComPtr<ID3D11DeviceContext> pD3dContext{};
    D3D_FEATURE_LEVEL featureLevel{};
    HRESULT hr = D3D11CreateDevice(nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        0,
        DEFAULT_DX_DEVICE_FLAG,
        0, 0,
        D3D11_SDK_VERSION,
        pDevice.GetAddressOf(), &featureLevel, pD3dContext.GetAddressOf());
    eh::ExitIfFailed(hr, "Failed to Create D3D Device!");
    eh::ExitIfFailed(featureLevel == D3D_FEATURE_LEVEL_11_0, "Create D3D Device Sucess but the feature level is incorrect!");
    LOGI("Initialize D3D D3DContext and Device successed, current feature level is {}", static_cast<int>(featureLevel));
    return { pDevice, pD3dContext };
}

UINT GetD3DMSAAQuality(const ComPtr<ID3D11Device> pDevice) {
    UINT quality{};
    auto hr = pDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &quality);
    eh::ExitIfFailed(hr, "Failed to check multisample level!");
    LOGI("Current physic device's msaa quality is {}", static_cast<int>(quality));
    return quality;
}

inline ComPtr<IDXGISwapChain> CreateD3DSwapChain(const ComPtr<ID3D11Device> pDevice, const HWND winId, const UINT quality) {
    LOGI("The multisample level is {}", static_cast<int>(quality));
    DXGI_SWAP_CHAIN_DESC scDesc{};
    scDesc.BufferDesc.Width = GAME_WIN_WIDTH;
    scDesc.BufferDesc.Height = GAME_WIN_HEIGHT;
    scDesc.BufferDesc.RefreshRate.Numerator = 60;
    scDesc.BufferDesc.RefreshRate.Denominator = 1;
    scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    scDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    if constexpr (GAME_ENABLE_MSAA) {
        scDesc.SampleDesc.Count = 4;
        scDesc.SampleDesc.Quality = quality - 1;
    }
    else {
        scDesc.SampleDesc.Count = 1;
        scDesc.SampleDesc.Quality = 0;
    }

    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 1;
    scDesc.Windowed = true;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    scDesc.Flags = 0;
    scDesc.OutputWindow = winId;

    ComPtr <IDXGIDevice> pDxgiDevice{};
    auto hr = pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(pDxgiDevice.GetAddressOf()));
    eh::ExitIfFailed(hr, "Failed to get dxgi device!");

    ComPtr <IDXGIAdapter> pDxgiAdapter{};
    hr = pDxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(pDxgiAdapter.GetAddressOf()));
    eh::ExitIfFailed(hr, "Failed to get dxgi adapter!");

    ComPtr<IDXGIFactory> pDxgiFactory{};
    hr = pDxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(pDxgiFactory.GetAddressOf()));
    eh::ExitIfFailed(hr, "Failed to get dxgi factory!");
    
    IDXGISwapChain* pDxgiSwapChain{};
    hr = pDxgiFactory->CreateSwapChain(pDevice.Get(), &scDesc, &pDxgiSwapChain);
    eh::ExitIfFailed(hr, "Failed to get create swap chain!");
    LOGI("Create SwapChain successed");
    return pDxgiSwapChain;
}

inline ComPtr<ID3D11RenderTargetView> CreateD3DRenderTargetView(const ComPtr<IDXGISwapChain> pDxgiSwapChain, const ComPtr<ID3D11Device> pDevice) {
    ComPtr<ID3D11RenderTargetView> pTargetView{};
    ComPtr<ID3D11Texture2D> pBackBuffer{};
    pDxgiSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(pBackBuffer.GetAddressOf()));
    auto hr = pDevice->CreateRenderTargetView(pBackBuffer.Get(), 0, &pTargetView);
    eh::ExitIfFailed(hr, "Failed to create render target view!");
    LOGI("Create RenderTargetView successed");
    return pTargetView;
}

std::pair<ComPtr< ID3D11Texture2D>, ComPtr<ID3D11DepthStencilView>> D3DCreateRenderTexture(const ComPtr<ID3D11Device> pDevice, const UINT quality) {
    D3D11_TEXTURE2D_DESC depthTextureDesc{};
    depthTextureDesc.Width = GAME_WIN_WIDTH;
    depthTextureDesc.Height = GAME_WIN_HEIGHT;
    depthTextureDesc.MipLevels = 1;
    depthTextureDesc.ArraySize = 1;
    depthTextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    if (GAME_ENABLE_MSAA) {
        depthTextureDesc.SampleDesc.Count = 4;
        depthTextureDesc.SampleDesc.Quality = quality - 1;
    }
    else {
        depthTextureDesc.SampleDesc.Count = 1;
        depthTextureDesc.SampleDesc.Quality = 0;
    }

    depthTextureDesc.Usage = D3D11_USAGE_DEFAULT;
    depthTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthTextureDesc.CPUAccessFlags = 0;
    depthTextureDesc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> pDepthTexture{};
    ComPtr<ID3D11DepthStencilView> pDepthView{};
    pDevice->CreateTexture2D(&depthTextureDesc, 0, pDepthTexture.GetAddressOf());
    auto hr = pDevice->CreateDepthStencilView(pDepthTexture.Get(), 0, &pDepthView);
    eh::ExitIfFailed(hr, "Failed to create depth stencil view!");
    LOGI("Create texture and view successed");
    return { pDepthTexture, pDepthView };
}

inline void D3DSetupViewPort(ComPtr<ID3D11DeviceContext> pD3dContext) {
    D3D11_VIEWPORT viewPort{};
    viewPort.TopLeftX = 0;
    viewPort.TopLeftY = 0;
    viewPort.Width = GAME_WIN_WIDTH;
    viewPort.Height = GAME_WIN_HEIGHT;
    viewPort.MaxDepth = 1.0;
    viewPort.MinDepth = 0.0;
    pD3dContext->RSSetViewports(1, &viewPort);
}

Application::Application() {
    LOGI("Game Start!");
}

Application::~Application() {
    LOGI("Game End!");
}

void Application::initD3DEnv(const HWND winId) {
    std::tie(_pd3dDevice, _pd3dDeviceCtx) = CreateD3DContextAndDevice();
    const auto quality = GetD3DMSAAQuality(_pd3dDevice);
    _pd3dSwapChain = CreateD3DSwapChain(_pd3dDevice, winId, quality);
    _pd3dRenderTargetView = CreateD3DRenderTargetView(_pd3dSwapChain, _pd3dDevice);

    D3DSetupViewPort(_pd3dDeviceCtx);
    const auto [depthTexture, depthView] = D3DCreateRenderTexture(_pd3dDevice, quality);
    _pd3dDeviceCtx->OMSetRenderTargets(1, _pd3dRenderTargetView.GetAddressOf(), depthView.Get());
}

// 窗口过程函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
            EndPaint(hwnd, &ps);
        }
        return 0;
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
        "Hello, DX11!",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        GAME_WIN_WIDTH, GAME_WIN_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    eh::ExitIfFailed(_winId, "Failed to create window, the window handle is nullptr");
}

bool Application::init(const HINSTANCE hInstance){
    createMainWindow(hInstance);
    initD3DEnv(_winId);
    LOGI("Initialize D3D environment successed");
    return true;
}

int Application::run(const int nShowCmd){
    ShowWindow(_winId, nShowCmd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

void Application::onResize() {}

void Application::updateScene(const float dt) {}

void Application::drawScene() {}

void Application::onMouseDown() {}

void Application::onMouseUp() {}

void Application::onMouseMove() {}