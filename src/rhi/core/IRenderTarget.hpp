#pragma once
#include <cstdint>

namespace rhi {

class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;
    virtual bool create(int width, int height) = 0;
    virtual bool bind() = 0;
    virtual bool unbind() = 0;
    virtual void* colorTexture() = 0;   // 供采样/后处理读取
    virtual void* handle() = 0;
    virtual void release() = 0;
};

} // namespace rhi
