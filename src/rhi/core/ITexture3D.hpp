#pragma once
#include <cstdint>
#include "Common.hpp"
#include "ITexture2D.hpp"

namespace rhi {

struct TextureDataView3D {
    const void* data{nullptr};
    int width{0}, height{0}, depth{0};
    int channels{0};
};

class ITexture3D {
public:
    virtual ~ITexture3D() = default;
    virtual bool init(const TextureDataView3D& data) = 0;                            // 保留（旧签名）
    virtual bool initCube(const TextureDesc& desc,
                          const TextureDataView2D* faces) = 0;                       // 新增：6 面 cubemap（faces[0..5]）
    virtual bool createEmpty(const TextureDesc& desc, int width, int height) = 0;    // 新增：分配 6 面 cubemap 存储
    virtual void bind(unsigned int unit = 0) = 0;
    virtual void* handle() = 0;
    virtual bool valid() const = 0;
    virtual void release() = 0;
};

} // namespace rhi
