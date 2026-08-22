#pragma once
#include "rhi/core/IRenderer.hpp"
#include "rhi/dx12/DXHeader.hpp"

namespace rhi {

struct DXImGuiInitInfo {
    ID3D12Device* device{nullptr};
    ID3D12CommandQueue* queue{nullptr};
    ID3D12DescriptorHeap* srvHeap{nullptr};
};

std::shared_ptr<IRenderer> createDX12Renderer();

} // namespace rhi
