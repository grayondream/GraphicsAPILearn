#if defined(__APPLE__)

#include "MetalTexture3D.hpp"
#include "MetalFormat.hpp"
#include <cstring>

namespace rhi::mtl {

MetalTexture3D::MetalTexture3D(void* device)
    : _device((__bridge id<MTLDevice>)device) {}

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

void* MetalTexture3D::handle() { return (__bridge void*)_texture; }

void MetalTexture3D::release() {
    if (_sampler) { _sampler = nil; }
    if (_texture) { _texture = nil; }
}

void MetalTexture3D::genCubeMipmaps() {
    if (!_texture || _texture.mipmapLevelCount <= 1) return;

    id<MTLCommandQueue> queue = [_device newCommandQueue];
    if (!queue) return;

    id<MTLCommandBuffer> cmd = [queue commandBuffer];
    if (!cmd) return;

    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    if (!blit) return;

    [blit generateMipmapsForTexture:_texture];
    [blit endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];
}

bool MetalTexture3D::createCubeTexture(int width, int height, bool mipmapped) {
    MTLPixelFormat pixelFormat = ToMTLPixelFormat(_desc.format);
    MTLTextureDescriptor* desc = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:pixelFormat
                                                                                      width:width
                                                                                     height:height
                                                                                  mipmapped:mipmapped];
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLResourceStorageModeShared;

    _texture = [_device newTextureWithDescriptor:desc];
    return _texture != nil;
}

bool MetalTexture3D::uploadFace(int faceIndex, const void* data, int width, int height, int channels) {
    if (!_texture || !data || faceIndex < 0 || faceIndex > 5) return false;

    MTLPixelFormat pixelFormat = _texture.pixelFormat;
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
    [_texture replaceRegion:region
               mipmapLevel:0
                     slice:faceIndex
                 withBytes:data
               bytesPerRow:bytesPerRow
             bytesPerImage:bytesPerRow * height];
    return true;
}

MTLSamplerState* MetalTexture3D::createSampler() {
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

    return [_device newSamplerStateWithDescriptor:desc];
}

} // namespace rhi::mtl

#endif
