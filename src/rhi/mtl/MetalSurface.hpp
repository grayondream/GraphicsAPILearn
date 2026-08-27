#pragma once

#include "rhi/core/ISurface.hpp"

#if defined(__APPLE__)

#import <QuartzCore/CAMetalLayer.h>

namespace rhi::mtl {

class MetalSurface : public ISurface {
public:
    explicit MetalSurface(CAMetalLayer* layer) : _layer(layer) {}
    ~MetalSurface() override = default;

    void* nativeHandle() override { return (__bridge void*)_layer; }
    int width() const override {
        if (!_layer) return 0;
        return static_cast<int>(_layer.drawableSize.width);
    }
    int height() const override {
        if (!_layer) return 0;
        return static_cast<int>(_layer.drawableSize.height);
    }

    CAMetalLayer* layer() const { return _layer; }

private:
    CAMetalLayer* _layer{nullptr};
};

} // namespace rhi::mtl

#else // non-Apple fallback

namespace rhi::mtl {

class MetalSurface : public ISurface {
public:
    MetalSurface() = default;
    ~MetalSurface() override = default;

    void* nativeHandle() override { return nullptr; }
    int width() const override { return 0; }
    int height() const override { return 0; }
};

} // namespace rhi::mtl

#endif
