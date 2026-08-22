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

// ImGui overlay 初始化信息桥：DXRenderer 类定义仅存在于 DXBackend.cpp，window 层
// 经此下转型读取句柄（等价于直接调用 DXRenderer::imguiInitInfo）
bool GetDXImGuiInitInfo(const std::shared_ptr<IRenderer>& renderer, DXImGuiInitInfo& out);

} // namespace rhi
