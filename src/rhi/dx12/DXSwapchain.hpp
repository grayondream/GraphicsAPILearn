#pragma once
#include "rhi/dx12/DXHeader.hpp"
#include "rhi/core/ISwapchain.hpp"
#include <memory>

namespace rhi {

class ISurface;

// 标准 DXGI flip-model 交换链：CreateDXGIFactory2 → CreateSwapChainForHwnd
// （HWND 来自 surface 的 GLFWwindow* 经 glfwGetWin32Window；FLIP_DISCARD、
// R8G8B8A8_UNORM）。持有 backbuffer RTV 与窗口深度 DSV。
// 呈现编排（Close cmdlist → ExecuteCommandLists → Present(1,0) → fence 单调
// 推进）由 DXRenderer::present 完成，本类只管 DXGI 对象与描述符生命周期；
// fence/event 由 Renderer 持有并借用传入（与 DXBuffer upload 共享同一 fence，
// Signal 一律 GetCompletedValue()+1 推进）。
class DXSwapchain : public ISwapchain {
public:
    ~DXSwapchain() override;

    bool init(ID3D12Device* device, ID3D12CommandQueue* queue,
              ID3D12Fence* frameFence, HANDLE fenceEvent,
              const std::shared_ptr<ISurface>& surface);
    void shutdown();

    bool present() override;   // Present(1,0)，命令须已 Execute
    void resize(int width, int height) override;
    void* handle() override;

    bool initialized() const { return _initialized; }
    uint32_t bufferCount() const { return kBufferCount; }
    uint32_t currentIndex() const;   // GetCurrentBackBufferIndex
    int width() const { return _width; }
    int height() const { return _height; }
    DXGI_FORMAT colorFormat() const { return DXGI_FORMAT_R8G8B8A8_UNORM; }
    D3D12_CPU_DESCRIPTOR_HANDLE rtv(uint32_t index);
    D3D12_CPU_DESCRIPTOR_HANDLE dsv();
    ID3D12Resource* backBuffer(uint32_t index);

    // GPU 空闲等待：fence GetCompletedValue()+1 单调推进并阻塞至完成
    // （ResizeBuffers 前必须调用，确保无在途命令引用 backbuffer）
    void waitForGpuIdle();

private:
    bool createSizeDependent(int width, int height);   // GetBuffer+RTV+深度 DSV
    void destroySizeDependent();

    static constexpr UINT kBufferCount = 2;

    ID3D12Device* _device{nullptr};
    ID3D12CommandQueue* _queue{nullptr};
    ID3D12Fence* _frameFence{nullptr};
    HANDLE _fenceEvent{nullptr};
    ComPtr<IDXGIFactory4> _factory;
    ComPtr<IDXGISwapChain3> _swapchain;
    ComPtr<ID3D12DescriptorHeap> _rtvHeap;
    ComPtr<ID3D12Resource> _buffers[kBufferCount];
    ComPtr<ID3D12DescriptorHeap> _dsvHeap;
    ComPtr<ID3D12Resource> _depth;
    UINT _rtvSize{0};
    UINT _dsvSize{0};
    int _width{0};
    int _height{0};
    bool _initialized{false};
};

} // namespace rhi
