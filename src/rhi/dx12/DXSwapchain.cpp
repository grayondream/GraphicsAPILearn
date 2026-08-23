#include "rhi/dx12/DXSwapchain.hpp"
#include "rhi/core/ISurface.hpp"
#include "base/Log.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace rhi {

DXSwapchain::~DXSwapchain() { shutdown(); }

bool DXSwapchain::init(ID3D12Device* device, ID3D12CommandQueue* queue,
                       ID3D12Fence* frameFence, HANDLE fenceEvent,
                       const std::shared_ptr<ISurface>& surface) {
    _device = device;
    _queue = queue;
    _frameFence = frameFence;
    _fenceEvent = fenceEvent;
    if (!_device || !_queue || !_frameFence || !_fenceEvent || !surface) return false;

    auto* window = static_cast<GLFWwindow*>(surface->nativeHandle());
    HWND hwnd = window ? glfwGetWin32Window(window) : nullptr;
    if (!hwnd) {
        LOGE("[DX12] glfwGetWin32Window failed");
        return false;
    }

    DX_CHECK(CreateDXGIFactory2(0, IID_PPV_ARGS(&_factory)), "CreateDXGIFactory2");
    if (!_factory.Get()) return false;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = static_cast<UINT>(surface->width() > 0 ? surface->width() : 1);
    desc.Height = static_cast<UINT>(surface->height() > 0 ? surface->height() : 1);
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;   // flip model 强制单采样，MSAA 走离屏 RT（Task 8）
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kBufferCount;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

    ComPtr<IDXGISwapChain1> sc1;
    DX_CHECK(_factory->CreateSwapChainForHwnd(_queue, hwnd, &desc, nullptr, nullptr, &sc1),
             "CreateSwapChainForHwnd");
    if (!sc1.Get()) return false;
    DX_CHECK(sc1->QueryInterface(IID_PPV_ARGS(&_swapchain)), "query IDXGISwapChain3");
    if (!_swapchain.Get()) return false;

    // 禁 Alt+Enter 独占全屏切换：窗口化 flip 链被系统接管后 RTV 全部失效
    _factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    _width = surface->width();
    _height = surface->height();
    if (!createSizeDependent(desc.Width, desc.Height)) return false;
    _initialized = true;
    LOGI("[DX12] swapchain ready {}x{} buffers={}", _width, _height, kBufferCount);
    return true;
}

void DXSwapchain::shutdown() {
    if (_queue && _frameFence) waitForGpuIdle();
    destroySizeDependent();
    if (_swapchain.Get()) { _swapchain->Release(); _swapchain.ptr = nullptr; }
    if (_factory.Get()) { _factory->Release(); _factory.ptr = nullptr; }
    _device = nullptr;
    _queue = nullptr;
    _frameFence = nullptr;
    _fenceEvent = nullptr;
    _initialized = false;
}

// 尺寸相关资源：backbuffer 引用 + RTV 堆、窗口深度 + DSV 堆。
// ResizeBuffers/重建前必须全部释放对 backbuffer 的引用。
bool DXSwapchain::createSizeDependent(int width, int height) {
    destroySizeDependent();

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = kBufferCount;
    DX_CHECK(_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&_rtvHeap)), "create RTV heap");
    if (!_rtvHeap.Get()) return false;
    _rtvSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for (UINT i = 0; i < kBufferCount; ++i) {
        DX_CHECK(_swapchain->GetBuffer(i, IID_PPV_ARGS(&_buffers[i])), "GetBuffer");
        if (!_buffers[i].Get()) return false;
        _device->CreateRenderTargetView(_buffers[i].Get(), nullptr, rtv(i));
    }

    // 窗口深度：GL 默认帧缓冲语义（clearColor 同清深度/模板；模板供 TemplateTest 等）
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Alignment = 0;
    rd.Width = static_cast<UINT64>(width);
    rd.Height = static_cast<UINT>(height);
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    DX_CHECK(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                              D3D12_RESOURCE_STATE_DEPTH_WRITE, nullptr,
                                              IID_PPV_ARGS(&_depth)),
             "create window depth buffer");

    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{};
    dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDesc.NumDescriptors = 1;
    DX_CHECK(_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&_dsvHeap)), "create DSV heap");
    if (!_dsvHeap.Get() || !_depth.Get()) return false;
    _dsvSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    _device->CreateDepthStencilView(_depth.Get(), nullptr, dsv());
    return true;
}

void DXSwapchain::destroySizeDependent() {
    for (auto& b : _buffers) {
        if (b.Get()) { b->Release(); b.ptr = nullptr; }
    }
    if (_rtvHeap.Get()) { _rtvHeap->Release(); _rtvHeap.ptr = nullptr; }
    if (_depth.Get()) { _depth->Release(); _depth.ptr = nullptr; }
    if (_dsvHeap.Get()) { _dsvHeap->Release(); _dsvHeap.ptr = nullptr; }
}

bool DXSwapchain::present() {
    if (!_initialized || !_swapchain.Get()) return false;
    HRESULT hr = _swapchain->Present(1, 0);   // vsync：与 VK FIFO/GL swapInterval 对齐
    if (FAILED(hr)) LOGE("[DX12] Present failed hr=0x{:08X}", static_cast<uint32_t>(hr));
    return SUCCEEDED(hr);
}

void DXSwapchain::resize(int width, int height) {
    if (!_initialized || width <= 0 || height <= 0 ||
        (width == _width && height == _height)) return;
    // 【终审 F5】当前为死代码（三后端 swapchain resize 均无调用点，行为一致）。
    // 若将来接线，renderer 侧必须同步复位 _backBufferBound=false / _omPending=true
    // （经 renderer 中转或回调）：否则新 backbuffer 从 PRESENT 出发却跳过
    // PRESENT→RT 屏障，present 尾部还会对新缓冲发 RT→PRESENT 无效屏障。
    // ResizeBuffers 要求 GPU 空闲且无 backbuffer 引用
    waitForGpuIdle();
    destroySizeDependent();
    DX_CHECK(_swapchain->ResizeBuffers(kBufferCount, static_cast<UINT>(width),
                                       static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, 0),
             "ResizeBuffers");
    _width = width;
    _height = height;
    // 失败不再吞返回值：backbuffer/DSV 缺失时 activateWindowTargets 会因空指针
    // 静默跳过窗口路径，此处 LOGE 留下定位线索
    if (!createSizeDependent(width, height)) {
        LOGE("[DX12] resize({}x{}): createSizeDependent failed; window targets unavailable",
             width, height);
    }
}

uint32_t DXSwapchain::currentIndex() const {
    return _swapchain.Get() ? static_cast<uint32_t>(_swapchain->GetCurrentBackBufferIndex())
                            : 0u;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXSwapchain::rtv(uint32_t index) {
    D3D12_CPU_DESCRIPTOR_HANDLE h{};
    if (_rtvHeap.Get()) h = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
    if (index > 0 && h.ptr != 0) h.ptr += static_cast<SIZE_T>(index) * _rtvSize;
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXSwapchain::dsv() {
    D3D12_CPU_DESCRIPTOR_HANDLE h{};
    if (_dsvHeap.Get()) h = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
    return h;
}

ID3D12Resource* DXSwapchain::backBuffer(uint32_t index) {
    return index < kBufferCount ? _buffers[index].Get() : nullptr;
}

void* DXSwapchain::handle() { return _swapchain.Get(); }

void DXSwapchain::waitForGpuIdle() {
    if (!_queue || !_frameFence || !_fenceEvent) return;
    // 共享 fence 单调约定（同 DXBuffer）：Signal 必须 GetCompletedValue()+1 推进
    const UINT64 value = _frameFence->GetCompletedValue() + 1;
    _queue->Signal(_frameFence, value);
    if (_frameFence->GetCompletedValue() < value) {
        _frameFence->SetEventOnCompletion(value, _fenceEvent);
        WaitForSingleObject(_fenceEvent, INFINITE);
    }
}

} // namespace rhi
