#pragma once
#include <cstdint>

namespace rhi {

struct TextureDataView3D {
    const void* data{nullptr};
    int width{0}, height{0}, depth{0};
    int channels{0};
};

class ITexture3D {
public:
    virtual ~ITexture3D() = default;
    virtual bool init(const TextureDataView3D& data) = 0;
    virtual void bind(unsigned int unit = 0) = 0;
    virtual void* handle() = 0;
    virtual bool valid() const = 0;
    virtual void release() = 0;
};

} // namespace rhi
