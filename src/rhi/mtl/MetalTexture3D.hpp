#pragma once

#include "rhi/core/ITexture3D.hpp"
#include <cstdint>

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
    bool valid() const override { return _texture != nullptr; }
    void release() override;
    void genCubeMipmaps() override;

    void* texture() const { return _texture; }
    void* sampler() const { return _sampler; }
    int width() const { return _width; }
    int height() const { return _height; }

private:
    bool createCubeTexture(int width, int height, bool mipmapped);
    bool uploadFace(int faceIndex, const void* data, int width, int height, int channels);
    void* createSampler();

    void* _device{nullptr};
    void* _texture{nullptr};
    void* _sampler{nullptr};
    TextureDesc _desc{};
    int _width{0};
    int _height{0};
    unsigned int _unit{0};
};

} // namespace rhi::mtl
