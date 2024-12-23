#include "Utils.hpp"
#include "EH/ErrorHandle.hpp"
#include "Base/GameTimer.hpp"
#include "Base/Log.hpp"
namespace Utils{
    namespace eh = ErrorHandle;
    using namespace base::log;
std::tuple<ComPtr<ID3D11Device>, ComPtr<ID3D11DeviceContext>, ComPtr<IDXGISwapChain>> CreateD3DDeviceAndtSwapChain(const HWND winId, const int width, const int height) {
    ComPtr<ID3D11Device> pdevice{};
    ComPtr<ID3D11DeviceContext> pcontext{}; 
    ComPtr<IDXGISwapChain> pswapChain{};
    // create a struct to hold information about the swap chain
    DXGI_SWAP_CHAIN_DESC scd{};
    // clear out the struct for use
    ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));
    // fill the swap chain description struct
    scd.BufferCount = 1;                                   // one back buffer
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;    // use 32-bit color
    scd.BufferDesc.Width = width;                   // set the back buffer width
    scd.BufferDesc.Height = height;                 // set the back buffer height
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;     // how swap chain is to be used
    scd.OutputWindow = winId;                               // the window to be used
    scd.SampleDesc.Count = 4;                              // how many multisamples
    scd.Windowed = TRUE;                                   // windowed/full-screen mode
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;    // allow full-screen switching

    // create a device, device context and swap chain using the information in the scd struct
    const auto hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        0,
        NULL,
        0,
        D3D11_SDK_VERSION,
        &scd,
        pswapChain.GetAddressOf(),
        pdevice.GetAddressOf(),
        NULL,
        pcontext.GetAddressOf());
    eh::ExitIfFailed(hr, "Failed to create device , context and swapchain!");
    return { pdevice, pcontext, pswapChain };
}

UINT GetD3DMSAAQuality(const ComPtr<ID3D11Device> pDevice) {
    UINT quality{};
    auto hr = pDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &quality);
    eh::ExitIfFailed(hr, "Failed to check multisample level!");
    LOGI("Current physic device's msaa quality is {}", static_cast<int>(quality));
    return quality;
}

ComPtr<ID3D11RenderTargetView> CreateD3DRenderTargetView(const ComPtr<IDXGISwapChain> pDxgiSwapChain, const ComPtr<ID3D11Device> pDevice) {
    ComPtr<ID3D11RenderTargetView> pTargetView{};
    ComPtr<ID3D11Texture2D> pBackBuffer{};
    auto hr = pDxgiSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(pBackBuffer.GetAddressOf()));
    eh::ExitIfFailed(hr, "Failed to get back buffer!");

    hr = pDevice->CreateRenderTargetView(pBackBuffer.Get(), 0, pTargetView.GetAddressOf());
    eh::ExitIfFailed(hr, "Failed to create render target view!");
    LOGI("Create RenderTargetView successed");
    return pTargetView;
}

std::pair<ComPtr< ID3D11Texture2D>, ComPtr<ID3D11DepthStencilView>> D3DCreateRenderTexture(const ComPtr<ID3D11Device> pDevice, const UINT quality, const int width, const int height, const bool enableMssaa) {
    D3D11_TEXTURE2D_DESC depthTextureDesc{};
    depthTextureDesc.Width = width;
    depthTextureDesc.Height = height;
    depthTextureDesc.MipLevels = 1;
    depthTextureDesc.ArraySize = 1;
    depthTextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    if (enableMssaa) {
        depthTextureDesc.SampleDesc.Count = 4;
        depthTextureDesc.SampleDesc.Quality = std::max<int>(0u, quality - 1);
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
    auto hr = pDevice->CreateDepthStencilView(pDepthTexture.Get(), 0, pDepthView.GetAddressOf());
    eh::ExitIfFailed(hr, "Failed to create depth stencil view!");
    LOGI("Create texture and view successed");
    return { pDepthTexture, pDepthView };
}

void D3DSetupViewPort(ComPtr<ID3D11DeviceContext> pD3dContext, const int width, const int height) {
    D3D11_VIEWPORT viewPort{};
    viewPort.TopLeftX = 0;
    viewPort.TopLeftY = 0;
    viewPort.Width = width;
    viewPort.Height = height;
    pD3dContext->RSSetViewports(1, &viewPort);
}

}