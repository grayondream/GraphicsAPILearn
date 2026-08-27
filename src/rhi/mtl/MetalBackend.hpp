#pragma once
#include "rhi/core/IRenderer.hpp"

#if defined(__APPLE__)

#import <Metal/Metal.h>

namespace rhi::mtl {

struct MetalImGuiInitInfo {
    id<MTLDevice> device{nil};
    id<MTLCommandQueue> queue{nil};
};

std::shared_ptr<IRenderer> createMetalRenderer();

bool GetMetalImGuiInitInfo(const std::shared_ptr<IRenderer>& renderer, MetalImGuiInitInfo& out);

} // namespace rhi::mtl

#else // non-Apple fallback

namespace rhi::mtl {

struct MetalImGuiInitInfo {};

inline std::shared_ptr<IRenderer> createMetalRenderer() { return nullptr; }

inline bool GetMetalImGuiInitInfo(const std::shared_ptr<IRenderer>&, MetalImGuiInitInfo&) { return false; }

} // namespace rhi::mtl

#endif
