#if defined(__APPLE__)

#include "MetalTexture3D.hpp"
#include "MetalFormat.hpp"

#include <cstring>
#include <cmath>

namespace rhi::mtl {

MetalTexture3D::MetalTexture3D(void* device)
    : _device(device) {}

MetalTexture3D::~MetalTexture3D() { release(); }

bool MetalTexture3D::init(const TextureDataView3D& data) {
    return false;
}

bool MetalTexture3D::initCube(const TextureDesc& desc, const TextureDataView2D* faces) {
    if (!faces) return false;

    _desc = desc;
    _width = faces[0].width;
    _height = faces[0].height;

    bool mipmapped = desc.generateMipmap;
    if (!createCubeTexture(_width, _height, mipmapped)) return false;

    for (int i = 0; i < 6; ++i) {
        if (!uploadFace(i, faces[i].data, faces[i].width, faces[i].height, faces[i].channels))
            return false;
    }

    if (mipmapped) genCubeMipmaps();
    _sampler = createSampler();
    return true;
}

bool MetalTexture3D::createEmpty(const TextureDesc& desc, int width, int height) {
    if (width <= 0 || height <= 0) return false;

    _desc = desc;
    _width = width;
    _height = height;

    if (!createCubeTexture(width, height, false)) return false;
    _sampler = createSampler();
    return true;
}

void MetalTexture3D::bind(unsigned int unit) { _unit = unit; }

void* MetalTexture3D::handle() { return _texture; }

void MetalTexture3D::release() {
    if (_sampler) { _sampler = nullptr; }
    if (_texture) { _texture = nullptr; }
}

void MetalTexture3D::genCubeMipmaps() {
    id<MTLTexture> tex = (__bridge id<MTLTexture>)_texture;
    if (!tex || tex.mipmapLevelCount <= 1) return;

    id<MTLDevice> device = (__bridge id<MTLDevice>)_device;
    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (!queue) return;

    id<MTLCommandBuffer> cmd = [queue commandBuffer];
    if (!cmd) return;

    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    if (!blit) return;

    [blit generateMipmapsForTexture:tex];
    [blit endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];
}

bool MetalTexture3D::createCubeTexture(int width, int height, bool mipmapped) {
    id<MTLDevice> device = (__bridge id<MTLDevice>)_device;
    MTLPixelFormat pixelFormat = ToMTLPixelFormat(_desc.format);
    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.textureType = MTLTextureTypeCube;
    desc.pixelFormat = pixelFormat;
    desc.width = width;
    desc.height = height;
    if (mipmapped) {
        desc.mipmapLevelCount = static_cast<NSUInteger>(std::floor(std::log2(static_cast<double>(std::max(width, height)))) + 1.0);
    }
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;

    id<MTLTexture> tex = [device newTextureWithDescriptor:desc];
    _texture = (__bridge void*)tex;
    return _texture != nullptr;
}

bool MetalTexture3D::uploadFace(int faceIndex, const void* data, int width, int height, int channels) {
    id<MTLTexture> tex = (__bridge id<MTLTexture>)_texture;
    if (!tex || !data || faceIndex < 0 || faceIndex > 5) return false;

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
                 slice:faceIndex
             withBytes:data
           bytesPerRow:bytesPerRow
         bytesPerImage:bytesPerRow * height];
    return true;
}

void* MetalTexture3D::createSampler() {
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
    desc.label = @"MetalTexture3D Sampler";

    id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:desc];
    return (__bridge void*)sampler;
}

} // namespace rhi::mtl

#endif
