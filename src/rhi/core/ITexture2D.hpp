#pragma once
#include <cstdint>
#include "Common.hpp"

namespace rhi {

struct TextureDataView2D {
    const void* data{nullptr};
    int width{0}, height{0};
    int channels{0};
};

class ITexture2D {
public:
    virtual ~ITexture2D() = default;
    virtual bool init(const TextureDataView2D& data) = 0;                            // 保留（旧签名）
    virtual bool init(const TextureDesc& desc, const TextureDataView2D& data) = 0;   // 新增
    virtual bool createEmpty(const TextureDesc& desc, int width, int height) = 0;    // 新增（渲染目标/深度纹理）
    virtual void bind(unsigned int unit = 0) = 0;
    virtual void* handle() = 0;
    virtual bool valid() const = 0;
    virtual void release() = 0;
};

} // namespace rhi
