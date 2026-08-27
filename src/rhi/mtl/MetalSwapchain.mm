#if defined(__APPLE__)

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "MetalSwapchain.hpp"

namespace rhi::mtl {

MetalSwapchain::~MetalSwapchain() {
    _drawable = nil;
}

void MetalSwapchain::init(CAMetalLayer* layer, int width, int height) {
    _layer = layer;
    _layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    _layer.framebufferOnly = YES;
    _layer.drawableSize = CGSizeMake(width, height);
    _layer.opaque = YES;
}

bool MetalSwapchain::present() {
    if (!_drawable) return false;
    [_drawable present];
    _drawable = nil;
    return true;
}

void MetalSwapchain::resize(int width, int height) {
    if (_layer) {
        _layer.drawableSize = CGSizeMake(width, height);
    }
}

void* MetalSwapchain::handle() {
    return (__bridge void*)_layer;
}

id<CAMetalDrawable> MetalSwapchain::currentDrawable() {
    if (!_layer) return nil;
    _drawable = [_layer nextDrawable];
    return _drawable;
}

} // namespace rhi::mtl

#endif
