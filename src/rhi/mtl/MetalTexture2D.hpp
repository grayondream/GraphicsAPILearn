#pragma once

#include "rhi/core/ITexture2D.hpp"
#include <cstdint>

namespace rhi::mtl {

class MetalTexture2D : public ITexture2D {
public:
    explicit MetalTexture2D(void* device);
    ~MetalTexture2D() override;

    bool init(const TextureDataView2D& data) override;
    bool init(const TextureDesc& desc, const TextureDataView2D& data) override;
    bool createEmpty(const TextureDesc& desc, int width, int height) override;
    void bind(unsigned int unit = 0) override;
    void* handle() override;
    bool valid() const override { return _texture != nullptr; }
    void release() override;

    void* texture() const { return _texture; }
    void* sampler() const { return _sampler; }
    int width() const { return _width; }
    int height() const { return _height; }

private:
    bool createTexture(int width, int height, bool mipmapped);
    bool uploadData(const void* data, int width, int height, int channels);
    bool generateMipmaps();
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
