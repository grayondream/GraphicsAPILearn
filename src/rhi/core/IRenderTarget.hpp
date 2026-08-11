#pragma once
#include <cstdint>
#include "Common.hpp"

namespace rhi {

class ITexture2D;
class ITexture3D;

class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;
    virtual bool create(int width, int height) = 0;                          // 保留（旧，默认 RGBA8 + Depth24Stencil8 RBO）
    virtual bool create(const FramebufferDesc& desc) = 0;                    // 新增：多 attachment + samples
    virtual bool attachCubeFace(ITexture3D* cube, int face, int mip = 0) = 0; // 新增：IBL 动态挂接
    virtual bool bind() = 0;
    virtual bool unbind() = 0;
    virtual void* colorTexture() = 0;                                        // 保留（旧，返回句柄）
    virtual ITexture2D* colorTexture2D(int attachment = 0) = 0;              // 新增：可采样接口指针
    virtual ITexture2D* depthTexture2D() = 0;                                // 新增：深度纹理可采样
    virtual bool resolveTo(IRenderTarget& dst) = 0;                          // 新增：MSAA blit resolve
    virtual void* handle() = 0;
    virtual void release() = 0;
};

} // namespace rhi
