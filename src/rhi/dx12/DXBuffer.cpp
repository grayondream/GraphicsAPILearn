#include "rhi/dx12/DXBuffer.hpp"
#include <cstring>

namespace rhi {

namespace {

size_t AlignUp(size_t v, size_t a) { return (v + a - 1) / a * a; }

} // namespace

DXBuffer::DXBuffer(ID3D12Device* device, ID3D12CommandQueue* queue,
                   ID3D12CommandAllocator* uploadAlloc, ID3D12Fence* uploadFence, HANDLE fenceEvent)
    : _device(device)
    , _queue(queue)
    , _uploadAlloc(uploadAlloc)
    , _uploadFence(uploadFence)
    , _fenceEvent(fenceEvent) {}

DXBuffer::~DXBuffer() {
    if (_mapped && _resource.Get()) {
        _resource->Unmap(0, nullptr);
        _mapped = nullptr;
    }
}

bool DXBuffer::init(const void* data, size_t size, BufferType type) {
    if (!_device || size == 0) return false;
    _type = type;
    _size = size;

    if (type == BufferType::Uniform) {
        if (!createUniformRing()) return false;
        // 初始数据落槽 0；后续每次 update 前进一格（同 VKBuffer 语义）
        if (data) std::memcpy(_mapped, data, size);
        return true;
    }

    // VB/IB 不走 SRV 路径，DENY_SHADER_RESOURCE 让驱动选择更优的内存放置
    if (!createCommitted(size)) return false;
    if (data && !copyViaStaging(data, size, 0)) return false;
    return true;
}

bool DXBuffer::createUniformRing() {
    _slotSize = AlignUp(sizeof(UniformBlock), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    const size_t total = _slotSize * kRingSlots;

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = total;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_UNKNOWN;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    DX_CHECK(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                              D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                              IID_PPV_ARGS(&_resource)),
             "create uniform ring buffer");
    if (!_resource.Get()) return false;

    // 读范围传空区间：声明 CPU 只写不读，避免驱动为回读同步整个堆
    D3D12_RANGE readRange{0, 0};
    DX_CHECK(_resource->Map(0, &readRange, &_mapped), "map uniform ring");
    return _mapped != nullptr;
}

bool DXBuffer::createCommitted(size_t size) {
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = size;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_UNKNOWN;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    rd.Flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    DX_CHECK(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                              D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                              IID_PPV_ARGS(&_resource)),
             "create default buffer");
    return _resource.Get() != nullptr;
}

bool DXBuffer::copyViaStaging(const void* data, size_t size, size_t dstOffset) {
    if (!_queue || !_uploadAlloc || !_uploadFence) return false;

    // 暂存 upload 资源即用即释放：仅本次拷贝的生命周期，析构在函数尾
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = size;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_UNKNOWN;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> staging;
    DX_CHECK(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                              D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                              IID_PPV_ARGS(&staging)),
             "create staging buffer");
    if (!staging.Get()) return false;

    void* mapped = nullptr;
    D3D12_RANGE readRange{0, 0};
    DX_CHECK(staging->Map(0, &readRange, &mapped), "map staging buffer");
    if (!mapped) return false;
    std::memcpy(mapped, data, size);
    staging->Unmap(0, nullptr);

    DX_CHECK(_uploadAlloc->Reset(), "reset upload allocator");
    ComPtr<ID3D12GraphicsCommandList> cmd;
    DX_CHECK(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _uploadAlloc,
                                        nullptr, IID_PPV_ARGS(&cmd)),
             "create upload command list");
    if (!cmd.Get()) return false;

    cmd->CopyBufferRegion(_resource.Get(), static_cast<UINT64>(dstOffset), staging.Get(), 0, size);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = _resource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = _type == BufferType::Index ? D3D12_RESOURCE_STATE_INDEX_BUFFER
                                                               : D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd->ResourceBarrier(1, &barrier);

    DX_CHECK(cmd->Close(), "close upload command list");
    ID3D12CommandList* lists[] = {cmd.Get()};
    _queue->ExecuteCommandLists(1, lists);

    waitForGpu(_uploadFence->GetCompletedValue() + 1);
    return true;
}

void DXBuffer::waitForGpu(UINT64 value) {
    _queue->Signal(_uploadFence, value);
    if (_uploadFence->GetCompletedValue() < value) {
        _uploadFence->SetEventOnCompletion(value, _fenceEvent);
        WaitForSingleObject(_fenceEvent, INFINITE);
    }
}

bool DXBuffer::update(const void* data, size_t size, size_t offset) {
    if (!_resource.Get() || !data) return false;

    if (_type == BufferType::Uniform && _mapped) {
        // 单次 update 不允许跨槽：越界会静默污染下一槽的同帧数据
        if (offset + size > _slotSize) {
            LOGE("[DX12] uniform write crosses slot boundary offset={} size={}", offset, size);
            return false;
        }
        ++_ringHead;
        const uint32_t slot = _ringHead % kRingSlots;
        // 帧界感知（终审 F4）：_uploadFence 即渲染器共享 _frameFence，present 尾部
        // waitForGpuIdle 必推进它；copyViaStaging 等一次性上传的 Signal+等待同理。
        // completed 值较上次 update 有增长 ⇒ 跨过一次全量同步，旧槽引用已全部执行
        // 完毕、覆写重新安全——在飞窗口内的连续 update 计数随之归零。
        if (_uploadFence) {
            const UINT64 completed = _uploadFence->GetCompletedValue();
            if (completed != _lastFenceCompleted) {
                _lastFenceCompleted = completed;
                _windowUpdates = 0;
            }
        }
        // 回绕守卫（终审 F4）：同一在飞窗口内第 kRingSlots+1 次 update 开始覆写本
        // 窗口内更早写入、尚被未提交绘制引用的槽。kRingSlots=256 为覆盖 LightSource
        // 批次的设计值（正常样例每帧个位数~百余次 update），== 判定保证每窗口至多
        // 告警一次、不阻断；跨帧回绕经 fence 全串行化天然安全，不在此列。
        if (++_windowUpdates == kRingSlots + 1) {
            LOGW("[DX12] UBO ring exhausted within one in-flight window (>{} updates): "
                 "oldest slots are being overwritten while earlier draws may still "
                 "reference them",
                 kRingSlots);
        }
        const size_t dst = slot * _slotSize + offset;
        std::memcpy(static_cast<char*>(_mapped) + dst, data, size);
        return true;
    }

    // 非 UBO 动态更新（当前 App 未用）：仍走一次性暂存拷贝保正确性。
    // 越界校验（终审 F8）：offset+size 超出缓冲会致 GPU 写越界，拒绝拷贝
    if (offset + size > _size) {
        LOGE("[DX12] vertex/index update out of range offset={} size={} buffer={}",
             offset, size, _size);
        return false;
    }
    return copyViaStaging(data, size, offset);
}

// CBV 的实际创建与绑定在渲染期由 Renderer 完成（Task 6，用 handle()+字节偏移），
// 此处无即时动作
bool DXBuffer::bindRange(uint32_t, size_t, size_t) { return true; }

bool DXBuffer::bind() { return true; }

void* DXBuffer::handle() { return _resource.Get(); }

void DXBuffer::BindAsVB(ID3D12GraphicsCommandList* cmdList, uint32_t slot, uint32_t stride) {
    if (!cmdList || !_resource.Get()) return;
    // stride 缓存须随参数失效（终审 F7）：同一 VB 换用不同步长的 layout 时重建视图
    if (!_vbViewReady || _vbView.StrideInBytes != static_cast<UINT>(stride)) {
        _vbView.BufferLocation = _resource->GetGPUVirtualAddress();
        _vbView.SizeInBytes = static_cast<UINT>(_size);
        _vbView.StrideInBytes = stride;
        _vbViewReady = true;
    }
    cmdList->IASetVertexBuffers(slot, 1, &_vbView);
}

void DXBuffer::BindAsIB(ID3D12GraphicsCommandList* cmdList) {
    if (!cmdList || !_resource.Get()) return;
    if (!_ibViewReady) {
        _ibView.BufferLocation = _resource->GetGPUVirtualAddress();
        _ibView.SizeInBytes = static_cast<UINT>(_size);
        // 项目索引统一 unsigned int（Shape/Mesh/RhiGeometry 全链路 GL_UNSIGNED_INT）
        _ibView.Format = DXGI_FORMAT_R32_UINT;
        _ibViewReady = true;
    }
    cmdList->IASetIndexBuffer(&_ibView);
}

size_t DXBuffer::submittedBase() const {
    return _type == BufferType::Uniform ? (_ringHead % kRingSlots) * _slotSize : 0;
}

} // namespace rhi
