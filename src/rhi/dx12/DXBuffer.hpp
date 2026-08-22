#pragma once
#include "rhi/dx12/DXHeader.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/UniformBlock.hpp"

namespace rhi {

// Vertex/Index：DEFAULT 堆 + 一次性 upload 暂存拷贝（fence 同步，暂存即用即释放）。
// Uniform：UPLOAD 堆持久映射 ring buffer（256 槽，槽大小按 D3D12 CBV 256 对齐约束
// 向上取整），每次 update 落到下一槽，避免同帧多 pass GPU 异步读被后写覆盖
// （对齐 VKBuffer 的 kRingSlots 方案；256 槽覆盖 LightSource 多实例逐 draw update
// 等一帧 >32 次 update 的样例）。
class DXBuffer : public IBuffer {
public:
    DXBuffer(ID3D12Device* device, ID3D12CommandQueue* queue,
             ID3D12CommandAllocator* uploadAlloc, ID3D12Fence* uploadFence, HANDLE fenceEvent);
    ~DXBuffer() override;

    bool init(const void* data, size_t size, BufferType type) override;
    bool update(const void* data, size_t size, size_t offset = 0) override;
    bool bindRange(uint32_t binding, size_t offset, size_t size) override;
    bool bind() override;
    void* handle() override;

    // stride 无法从 IBuffer 推得：Renderer 在 setVertexBuffer 时保存 layout，
    // 绑定时由 Renderer 传入（VertexElement.stride）
    void BindAsVB(ID3D12GraphicsCommandList* cmdList, uint32_t slot, uint32_t stride);
    void BindAsIB(ID3D12GraphicsCommandList* cmdList);

    // 最近一次 update 写入的槽基址（字节），Task 6 Renderer 用它换算 bindRange 偏移
    size_t submittedBase() const;

private:
    bool createCommitted(size_t size);
    bool createUniformRing();
    bool copyViaStaging(const void* data, size_t size, size_t dstOffset);
    void waitForGpu(UINT64 value);

    ID3D12Device* _device{nullptr};
    ID3D12CommandQueue* _queue{nullptr};
    ID3D12CommandAllocator* _uploadAlloc{nullptr};
    ID3D12Fence* _uploadFence{nullptr};
    HANDLE _fenceEvent{nullptr};

    BufferType _type{BufferType::Vertex};
    size_t _size{0};
    ComPtr<ID3D12Resource> _resource;

    static constexpr uint32_t kRingSlots = 256;
    size_t _slotSize{0};      // uniform 槽大小（256 对齐后的 sizeof(UniformBlock)）
    uint32_t _ringHead{0};    // 累计更新计数，模 kRingSlots 得当前槽
    void* _mapped{nullptr};   // uniform 的持久映射

    bool _vbViewReady{false};
    D3D12_VERTEX_BUFFER_VIEW _vbView{};
    bool _ibViewReady{false};
    D3D12_INDEX_BUFFER_VIEW _ibView{};
};

} // namespace rhi
