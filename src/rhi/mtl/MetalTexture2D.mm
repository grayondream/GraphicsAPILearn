#if defined(__APPLE__)

#include "MetalTexture2D.hpp"
#include "MetalFormat.hpp"
#import <Metal/Metal.h>
#include <cstring>

namespace rhi::mtl {

MetalTexture2D::MetalTexture2D(void* device)
    : _device(device) {}

MetalTexture2D::~MetalTexture2D() { release(); }

bool MetalTexture2D::init(const TextureDataView2D& data) {
    if (!data.data || data.width <= 0 || data.height <= 0) return false;

    _desc.format = (data.channels == 4) ? TextureFormat::RGBA8 : TextureFormat::RGB8;
    _width = data.width;
    _height = data.height;

    if (!createTexture(data.width, data.height, false)) return false;
    if (!uploadData(data.data, data.width, data.height, data.channels)) return false;
    _sampler = createSampler();
    return true;
}

bool MetalTexture2D::init(const TextureDesc& desc, const TextureDataView2D& data) {
    if (!data.data || data.width <= 0 || data.height <= 0) return false;

    _desc = desc;
    _width = data.width;
    _height = data.height;

    bool mipmapped = desc.generateMipmap && !desc.multisample;
    if (!createTexture(data.width, data.height, mipmapped)) return false;
    if (!uploadData(data.data, data.width, data.height, data.channels)) return false;
    if (mipmapped) generateMipmaps();
    _sampler = createSampler();
    return true;
}

bool MetalTexture2D::createEmpty(const TextureDesc& desc, int width, int height) {
    if (width <= 0 || height <= 0) return false;

    _desc = desc;
    _width = width;
    _height = height;

    if (!createTexture(width, height, false)) return false;
    _sampler = createSampler();
    return true;
}

void MetalTexture2D::bind(unsigned int unit) { _unit = unit; }

void* MetalTexture2D::handle() { return _texture; }

void MetalTexture2D::release() {
    if (_sampler) { _sampler = nullptr; }
    if (_texture) { _texture = nullptr; }
}

bool MetalTexture2D::createTexture(int width, int height, bool mipmapped) {
    id<MTLDevice> device = (__bridge id<MTLDevice>)_device;
    MTLPixelFormat pixelFormat = ToMTLPixelFormat(_desc.format);
    MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                                                                                     width:width
                                                                                    height:height
                                                                                 mipmapped:mipmapped];
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;

    if (_desc.multisample && _desc.samples > 1) {
        desc.sampleCount = _desc.samples;
        desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
    }

    id<MTLTexture> tex = [device newTextureWithDescriptor:desc];
    _texture = (__bridge void*)tex;
    return _texture != nullptr;
}

bool MetalTexture2D::uploadData(const void* data, int width, int height, int channels) {
    id<MTLTexture> tex = (__bridge id<MTLTexture>)_texture;
    if (!tex || !data) return false;

    MTLPixelFormat pixelFormat = tex.pixelFormat;
    size_t bytesPerRow = 0;
    if (pixelFormat == MTLPixelFormatRGBA8Unorm || pixelFormat == MTLPixelFormatBGRA8Unorm) {
        bytesPerRow = width * 4;
    } else if (pixelFormat == MTLPixelFormatRGBA16Float) {
        bytesPerRow = width * 8;
    } else if (pixelFormat == MTLPixelFormatRGBA32Float) {
        bytesPerRow = width * 16;
    } else if (pixelFormat == MTLPixelFormatRG16Float) {
        bytesPerRow = width * 4;
    } else if (pixelFormat == MTLPixelFormatR32Float) {
        bytesPerRow = width * 4;
    } else {
        bytesPerRow = width * channels;
    }

    MTLRegion region = MTLRegionMake2D(0, 0, width, height);
    [tex replaceRegion:region
           mipmapLevel:0
             withBytes:data
           bytesPerRow:bytesPerRow];
    return true;
}

bool MetalTexture2D::generateMipmaps() {
    id<MTLTexture> tex = (__bridge id<MTLTexture>)_texture;
    if (!tex || tex.mipmapLevelCount <= 1) return true;

    id<MTLDevice> device = (__bridge id<MTLDevice>)_device;
    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (!queue) return false;

    id<MTLCommandBuffer> cmd = [queue commandBuffer];
    if (!cmd) return false;

    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    if (!blit) return false;

    [blit generateMipmapsForTexture:tex];
    [blit endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];

    return true;
}

void* MetalTexture2D::createSampler() {
    id<MTLDevice> device = (__bridge id<MTLDevice>)_device;
    MTLSamplerDescriptor* desc = [[MTLSamplerDescriptor alloc] init];

    auto toMTLWrap = [](TextureWrap w) -> MTLSamplerAddressMode {
        switch (w) {
            case TextureWrap::Repeat:        return MTLSamplerAddressModeRepeat;
            case TextureWrap::ClampToEdge:   return MTLSamplerAddressModeClampToEdge;
            case TextureWrap::ClampToBorder: return MTLSamplerAddressModeClampToEdge;
        }
        return MTLSamplerAddressModeRepeat;
    };

    auto toMTLMinFilter = [](TextureFilter f) -> MTLSamplerMinMagFilter {
        switch (f) {
            case TextureFilter::Nearest:       return MTLSamplerMinMagFilterNearest;
            case TextureFilter::Linear:        return MTLSamplerMinMagFilterLinear;
            case TextureFilter::LinearMipLinear: return MTLSamplerMinMagFilterLinear;
        }
        return MTLSamplerMinMagFilterLinear;
    };

    auto toMTLMagFilter = [](TextureFilter f) -> MTLSamplerMinMagFilter {
        switch (f) {
            case TextureFilter::Nearest:       return MTLSamplerMinMagFilterNearest;
            case TextureFilter::Linear:        return MTLSamplerMinMagFilterLinear;
            case TextureFilter::LinearMipLinear: return MTLSamplerMinMagFilterLinear;
        }
        return MTLSamplerMinMagFilterLinear;
    };

    desc.sAddressMode = toMTLWrap(_desc.wrapS);
    desc.tAddressMode = toMTLWrap(_desc.wrapT);
    desc.minFilter = toMTLMinFilter(_desc.minFilter);
    desc.magFilter = toMTLMagFilter(_desc.magFilter);
    desc.mipFilter = (_desc.minFilter == TextureFilter::LinearMipLinear)
        ? MTLSamplerMipFilterLinear
        : MTLSamplerMipFilterNotMipmapped;
    desc.maxAnisotropy = 16;
    desc.label = @"MetalTexture2D Sampler";

    id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:desc];
    return (__bridge void*)sampler;
}

} // namespace rhi::mtl

#endif
