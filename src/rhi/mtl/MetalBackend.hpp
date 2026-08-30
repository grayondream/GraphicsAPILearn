#pragma once
#include "rhi/core/IRenderer.hpp"

namespace rhi::mtl {

// 使用不透明指针避免在 C++ 头文件中包含 Objective-C 头文件
// 实际类型在 .mm 文件中定义
struct MetalImGuiInitInfo {
    void* device{nullptr};
    void* commandQueue{nullptr};
    void* renderPassDescriptor{nullptr};
    void* commandBuffer{nullptr};
    void* renderEncoder{nullptr};
};

std::shared_ptr<IRenderer> createMetalRenderer();

bool GetMetalImGuiInitInfo(const std::shared_ptr<IRenderer>& renderer, MetalImGuiInitInfo& out);

} // namespace rhi::mtl
