#pragma once

#include "rhi/core/IRenderTarget.hpp"
#include <memory>
#include <vector>

#if defined(__APPLE__)

#import <Metal/Metal.h>

namespace rhi::mtl {

class MetalTexture2D;

class MetalRenderTarget : public IRenderTarget {
public:
    explicit MetalRenderTarget(void* device);
    ~MetalRenderTarget() override;

    bool create(int width, int height) override;
    bool create(const FramebufferDesc& desc) override;
    bool attachCubeFace(ITexture3D* cube, int face, int mip = 0) override;
    bool attachDepthCube(ITexture3D* cube, int mip = 0) override;
    bool bind() override;
    bool unbind() override;
    void* colorTexture() override;
    ITexture2D* colorTexture2D(int attachment = 0) override;
    ITexture2D* depthTexture2D() override;
    bool resolveTo(IRenderTarget& dst) override;
    void* handle() override;
    void release() override;

    id<MTLTexture> activeColorTexture() const;
    id<MTLTexture> activeDepthTexture() const;
    bool valid() const { return _valid; }

private:
    void buildRenderPassDescriptor();

    void* _device{nullptr};
    MTLRenderPassDescriptor* __strong _rpd{nil};
    FramebufferDesc _desc{};

    std::vector<std::shared_ptr<MetalTexture2D>> _colors;
    std::shared_ptr<MetalTexture2D> _depth;

    id<MTLTexture> __strong _cubeColorTexture{nil};
    id<MTLTexture> __strong _cubeDepthTexture{nil};
    ITexture3D* _cube{nullptr};
    int _face{-1};
    int _mip{0};
    bool _cubeIsDepth{false};

    int _width{0};
    int _height{0};
    int _samples{1};
    bool _valid{false};
};

} // namespace rhi::mtl

#else // non-Apple fallback

namespace rhi::mtl {

class MetalRenderTarget : public IRenderTarget {
public:
    MetalRenderTarget() = default;
    ~MetalRenderTarget() override = default;

    bool create(int, int) override { return false; }
    bool create(const FramebufferDesc&) override { return false; }
    bool attachCubeFace(ITexture3D*, int, int = 0) override { return false; }
    bool attachDepthCube(ITexture3D*, int = 0) override { return false; }
    bool bind() override { return false; }
    bool unbind() override { return false; }
    void* colorTexture() override { return nullptr; }
    ITexture2D* colorTexture2D(int = 0) override { return nullptr; }
    ITexture2D* depthTexture2D() override { return nullptr; }
    bool resolveTo(IRenderTarget&) override { return false; }
    void* handle() override { return nullptr; }
    void release() override {}
};

} // namespace rhi::mtl

#endif
