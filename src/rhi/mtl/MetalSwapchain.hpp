#pragma once

#include "rhi/core/ISwapchain.hpp"

#if defined(__APPLE__)

#import <QuartzCore/CAMetalLayer.h>

namespace rhi::mtl {

class MetalSwapchain : public ISwapchain {
public:
    MetalSwapchain() = default;
    ~MetalSwapchain() override;

    void init(CAMetalLayer* layer, int width, int height);
    bool present() override;
    void resize(int width, int height) override;
    void* handle() override;

    id<CAMetalDrawable> currentDrawable();

private:
    CAMetalLayer* _layer{nullptr};
    id<CAMetalDrawable> _drawable{nil};
};

} // namespace rhi::mtl

#else // non-Apple fallback

namespace rhi::mtl {

class MetalSwapchain : public ISwapchain {
public:
    MetalSwapchain() = default;
    ~MetalSwapchain() override = default;

    bool present() override { return false; }
    void resize(int, int) override {}
    void* handle() override { return nullptr; }
};

} // namespace rhi::mtl

#endif
