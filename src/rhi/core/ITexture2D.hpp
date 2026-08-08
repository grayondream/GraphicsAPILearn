#pragma once
#include <cstdint>

namespace rhi {

struct TextureDataView2D {
    const void* data{nullptr};
    int width{0}, height{0};
    int channels{0};
};

class ITexture2D {
public:
    virtual ~ITexture2D() = default;
    virtual bool init(const TextureDataView2D& data) = 0;
    virtual void bind(unsigned int unit = 0) = 0;
    virtual void* handle() = 0;
    virtual bool valid() const = 0;
    virtual void release() = 0;
};

} // namespace rhi
