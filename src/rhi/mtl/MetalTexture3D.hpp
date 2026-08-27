#pragma once

#include "rhi/core/ITexture3D.hpp"
#include <cstdint>

#if defined(__APPLE__)

#import <Metal/Metal.h>

namespace rhi::mtl {

class MetalTexture3D : public ITexture3D {
public:
    explicit MetalTexture3D(void* device);
    ~MetalTexture3D() override;

    bool init(const TextureDataView3D& data) override;
    bool initCube(const TextureDesc& desc, const TextureDataView2D* faces) override;
    bool createEmpty(const TextureDesc& desc, int width, int height) override;
    void bind(unsigned int unit = 0) override;
    void* handle() override;
    bool valid() const override { return _texture != nil; }
    void release() override;
    void genCubeMipmaps() override;

    id<MTLTexture> texture() const { return _texture; }
    id<MTLSamplerState> sampler() const { return _sampler; }
    int width() const { return _width; }
    int height() const { return _height; }

private:
    bool createCubeTexture(int width, int height, bool mipmapped);
    bool uploadFace(int faceIndex, const void* data, int width, int height, int channels);
    MTLSamplerState* createSampler();

    id<MTLDevice> _device{nil};
    id<MTLTexture> _texture{nil};
    id<MTLSamplerState> _sampler{nil};
    TextureDesc _desc{};
    int _width{0};
    int _height{0};
    unsigned int _unit{0};
};

} // namespace rhi::mtl

#else // non-Apple fallback

namespace rhi::mtl {

class MetalTexture3D : public ITexture3D {
public:
    MetalTexture3D() = default;
    ~MetalTexture3D() override = default;

    bool init(const TextureDataView3D&) override { return false; }
    bool initCube(const TextureDesc&, const TextureDataView2D*) override { return false; }
    bool createEmpty(const TextureDesc&, int, int) override { return false; }
    void bind(unsigned int = 0) override {}
    void* handle() override { return nullptr; }
    bool valid() const override { return false; }
    void release() override {}
    void genCubeMipmaps() override {}
};

} // namespace rhi::mtl

#endif
