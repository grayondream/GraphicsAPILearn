#include "rhi/dx11/DX11Backend.hpp"
#include "rhi/core/ISurface.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace rhi {

DX11Swapchain::~DX11Swapchain() { shutdown(); }

bool DX11Swapchain::init(ID3D11Device* device, const std::shared_ptr<ISurface>& surface) {
    _device = device;
    if (!_device || !surface) return false;

    auto* window = static_cast<GLFWwindow*>(surface->nativeHandle());
    HWND hwnd = window ? glfwGetWin32Window(window) : nullptr;
    if (!hwnd) {
        LOGE("[DX11] glfwGetWin32Window failed");
        return false;
    }

    DX11_CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&_factory)), "CreateDXGIFactory1");
    if (!_factory.Get()) return false;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = static_cast<UINT>(surface->width() > 0 ? surface->width() : 1);
    desc.Height = static_cast<UINT>(surface->height() > 0 ? surface->height() : 1);
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;   // brief 约定 BGRA（dump 读回换序）
    desc.SampleDesc.Count = 1;                  // flip 模型强制单采样，MSAA 走离屏 RT
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kBufferCount;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

    // FLIP_DISCARD 失败回退传统 DISCARD（brief 约定；老系统/特殊窗口样式兜底）
    HRESULT hrFlip = _factory->CreateSwapChainForHwnd(_device, hwnd, &desc, nullptr, nullptr, &_swapchain);
    DX11_CHECK(hrFlip, "CreateSwapChainForHwnd(FLIP_DISCARD)");
    if (!_swapchain.Get()) {
        LOGW("[DX11] FLIP_DISCARD swapchain unavailable (hr=0x{:08X}); retrying with legacy DISCARD",
             static_cast<uint32_t>(hrFlip));
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        DX11_CHECK(_factory->CreateSwapChainForHwnd(_device, hwnd, &desc, nullptr, nullptr, &_swapchain),
                   "CreateSwapChainForHwnd(DISCARD)");
        if (!_swapchain.Get()) return false;
    }

    // 禁 Alt+Enter 独占全屏切换：全屏接管后 RTV 全部失效（同 DX12）
    _factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    _width = surface->width();
    _height = surface->height();
    if (!createSizeDependent(desc.Width, desc.Height)) return false;
    _initialized = true;
    LOGI("[DX11] swapchain ready {}x{} buffers={}", _width, _height, kBufferCount);
    return true;
}

void DX11Swapchain::shutdown() {
    destroySizeDependent();
    _swapchain3.Reset();
    if (_swapchain.Get()) { _swapchain->Release(); _swapchain.ptr = nullptr; }
    if (_factory.Get()) { _factory->Release(); _factory.ptr = nullptr; }
    _device = nullptr;
    _initialized = false;
}

// 尺寸相关资源：backbuffer 引用 + 每缓冲 RTV、窗口深度纹理 + DSV。
// ResizeBuffers 前必须全部释放对 backbuffer 的引用。
bool DX11Swapchain::createSizeDependent(int width, int height) {
    destroySizeDependent();

    for (UINT i = 0; i < kBufferCount; ++i) {
        DX11_CHECK(_swapchain->GetBuffer(i, IID_PPV_ARGS(&_buffers[i])), "GetBuffer");
        if (!_buffers[i].Get()) return false;
        DX11_CHECK(_device->CreateRenderTargetView(_buffers[i].Get(), nullptr, &_rtv[i]),
                   "create backbuffer RTV");
        if (!_rtv[i].Get()) return false;
    }

    // 窗口深度：GL 默认帧缓冲语义（clearColor 同清深度/模板；模板供 TemplateTest 等）
    D3D11_TEXTURE2D_DESC dd{};
    dd.Width = static_cast<UINT>(width);
    dd.Height = static_cast<UINT>(height);
    dd.MipLevels = 1;
    dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1;
    dd.SampleDesc.Quality = 0;
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    dd.CPUAccessFlags = 0;
    dd.MiscFlags = 0;
    DX11_CHECK(_device->CreateTexture2D(&dd, nullptr, &_depth), "create window depth buffer");
    if (!_depth.Get()) return false;
    DX11_CHECK(_device->CreateDepthStencilView(_depth.Get(), nullptr, &_dsv),
               "create window DSV");
    return _dsv.Get() != nullptr;
}

void DX11Swapchain::destroySizeDependent() {
    for (UINT i = 0; i < kBufferCount; ++i) {
        if (_rtv[i].Get()) { _rtv[i]->Release(); _rtv[i].ptr = nullptr; }
        if (_buffers[i].Get()) { _buffers[i]->Release(); _buffers[i].ptr = nullptr; }
    }
    if (_dsv.Get()) { _dsv->Release(); _dsv.ptr = nullptr; }
    if (_depth.Get()) { _depth->Release(); _depth.ptr = nullptr; }
}

bool DX11Swapchain::present() {
    if (!_initialized || !_swapchain.Get()) return false;
    HRESULT hr = _swapchain->Present(1, 0);   // vsync：与 VK FIFO/GL swapInterval 对齐
    if (FAILED(hr)) LOGE("[DX11] Present failed hr=0x{:08X}", static_cast<uint32_t>(hr));
    return SUCCEEDED(hr);
}

void DX11Swapchain::resize(int width, int height) {
    if (!_initialized || width <= 0 || height <= 0 ||
        (width == _width && height == _height)) return;
    // 当前无调用点（三后端 resize 均为死代码，行为一致）。接线前提：
    // renderer 侧须同步复位 _windowBound=false 并重发 OM 绑定，否则新 backbuffer
    // 的 RTV 未激活。ResizeBuffers 要求 GPU 无 backbuffer 引用（即时上下文天然满足，
    // 先释放 RTV 即可）
    destroySizeDependent();
    DX11_CHECK(_swapchain->ResizeBuffers(kBufferCount, static_cast<UINT>(width),
                                         static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, 0),
               "ResizeBuffers");
    _width = width;
    _height = height;
    if (!createSizeDependent(width, height)) {
        LOGE("[DX11] resize({}x{}): createSizeDependent failed; window targets unavailable",
             width, height);
    }
}

uint32_t DX11Swapchain::currentIndex() const {
    return _swapchain3.Get()
               ? static_cast<uint32_t>(_swapchain3->GetCurrentBackBufferIndex())
               : 0u;
}

ID3D11RenderTargetView* DX11Swapchain::rtv(uint32_t index) {
    return index < kBufferCount ? _rtv[index].Get() : nullptr;
}

ID3D11DepthStencilView* DX11Swapchain::dsv() {
    return _dsv.Get();
}

ID3D11Texture2D* DX11Swapchain::backBuffer(uint32_t index) {
    return index < kBufferCount ? _buffers[index].Get() : nullptr;
}

void* DX11Swapchain::handle() { return _swapchain.Get(); }

} // namespace rhi
