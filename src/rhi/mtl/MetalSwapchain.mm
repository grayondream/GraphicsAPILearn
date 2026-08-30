#if defined(__APPLE__)

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "MetalSwapchain.hpp"

namespace rhi::mtl {

MetalSwapchain::~MetalSwapchain() {
    _drawable = nil;
    _depthTexture = nil;
    _device = nil;
    _layer = nullptr;
}

void MetalSwapchain::rebuildDepthTexture() {
    _depthTexture = nil;
    if (!_device || _width <= 0 || _height <= 0) return;
    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                           width:_width
                                                          height:_height
                                                       mipmapped:NO];
    desc.storageMode = MTLStorageModePrivate;
    desc.usage = MTLTextureUsageRenderTarget;
    _depthTexture = [_device newTextureWithDescriptor:desc];
}

void MetalSwapchain::init(CAMetalLayer* layer, int width, int height) {
    _layer = layer;
    _layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    _layer.framebufferOnly = getenv("METAL_READBACK") ? NO : YES;
    _layer.drawableSize = CGSizeMake(width, height);
    _layer.opaque = YES;
    _device = _layer.device;
    _width = width;
    _height = height;
    rebuildDepthTexture();
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
    if (width != _width || height != _height) {
        _width = width;
        _height = height;
        rebuildDepthTexture();
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
