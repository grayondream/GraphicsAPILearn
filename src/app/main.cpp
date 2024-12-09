#include <iostream>
#include <d3d11.h>
#include <Windows.h>
#include <format>
#include <cassert>
#include "EH/ErrorHandle.hpp"
#include "Base/GameTimer.hpp"

#if defined(DEBUG) || defined(_DEBUG)
#define DEFAULT_DX_DEVICE_FLAG D3D11_CREATE_DEVICE_DEBUG
#else
#define DEFAULT_DX_DEVICE_FLAG 0
#endif

static inline constexpr int GAME_ENABLE_MSAA = true;
static inline constexpr int GAME_WIN_WIDTH = 720;
static inline constexpr int GAME_WIN_HEIGHT = 480;
namespace eh = ErrorHandle;

int CreateD3DContext(HWND winId){
    ID3D11Device* pDevice{};
    ID3D11DeviceContext* pD3dContext{};
    D3D_FEATURE_LEVEL featureLevel{};
    HRESULT hr = D3D11CreateDevice( nullptr, 
                                    D3D_DRIVER_TYPE_HARDWARE, 
                                    0, 
                                    DEFAULT_DX_DEVICE_FLAG, 
                                    0, 0, 
                                    D3D11_SDK_VERSION, 
                                    &pDevice, &featureLevel, &pD3dContext);
    eh::ExitIfFailed(hr, "Failed to Create D3D Device!");
    eh::ExitIfFailed(featureLevel == D3D_FEATURE_LEVEL_11_0, "Create D3D Device Sucess but the feature level is incorrect!");
   
    UINT quality{};
    hr = pDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &quality);
    eh::ExitIfFailed(hr, "Failed to check multisample level!");
    

    assert(quality > 0);
    std::cout << std::format("The multisample level is {}", static_cast<int>(quality));
    DXGI_SWAP_CHAIN_DESC scDesc{};
    scDesc.BufferDesc.Width = GAME_WIN_WIDTH;
    scDesc.BufferDesc.Height = GAME_WIN_HEIGHT;
    scDesc.BufferDesc.RefreshRate.Numerator = 60;
    scDesc.BufferDesc.RefreshRate.Denominator = 1;
    scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferDesc.ScanlineOrdering - DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
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

    IDXGIDevice* pDxgiDevice{};
    hr = pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&pDxgiDevice));
    eh::ExitIfFailed(hr, "Failed to get dxgi device!");

    IDXGIAdapter* pDxgiAdapter{};
    hr = pDxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&pDxgiAdapter));
    eh::ExitIfFailed(hr, "Failed to get dxgi adapter!");
    
    IDXGIFactory* pDxgiFactory{};
    hr = pDxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&pDxgiFactory));
    eh::ExitIfFailed(hr, "Failed to get dxgi factory!");

    IDXGISwapChain* pDxgiSwapChain{};
    hr = pDxgiFactory->CreateSwapChain(pDevice, &scDesc, &pDxgiSwapChain);
    eh::ExitIfFailed(hr, "Failed to get create swap chain!");
    
    ID3D11RenderTargetView* pTargetView{};
    ID3D11Texture2D* pBackBuffer{};
    pDxgiSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
    hr = pDevice->CreateRenderTargetView(pBackBuffer, 0, &pTargetView);
    eh::ExitIfFailed(hr, "Failed to create render target view!");

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

    ID3D11Texture2D* pDepthTexture{};
    ID3D11DepthStencilView* pDepthView{};
    pDevice->CreateTexture2D(&depthTextureDesc, 0, &pDepthTexture);
    hr = pDevice->CreateDepthStencilView(pDepthTexture, 0, &pDepthView);
    eh::ExitIfFailed(hr, "Failed to create depth stencil view!");

    pD3dContext->OMSetRenderTargets(1, &pTargetView, pDepthView);

    D3D11_VIEWPORT viewPort{};
    viewPort.TopLeftX = 0;
    viewPort.TopLeftY = 0;
    viewPort.Width = GAME_WIN_WIDTH;
    viewPort.Height = GAME_WIN_HEIGHT;
    viewPort.MaxDepth = 1.0;
    viewPort.MinDepth = 0.0;
    pD3dContext->RSSetViewports(1, &viewPort);
    const auto v = GetSecondsPreCount();
    pDxgiFactory->Release();
    pDxgiAdapter->Release();
    pDxgiDevice->Release();
    pBackBuffer->Release();
    return 0;
}

#include <windows.h>

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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd) {
    const char CLASS_NAME[] = "Sample Window Class";

    // 注册窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    // 创建窗口
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "Hello, DX11!",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        GAME_WIN_WIDTH, GAME_WIN_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    eh::ExitIfFailed(hwnd, "Failed to create window, the window handle is nullptr");
    CreateD3DContext(hwnd);
    ShowWindow(hwnd, nShowCmd);

    // 进入消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}