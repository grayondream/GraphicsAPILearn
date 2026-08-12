#pragma once
#include "rhi/core/IRenderTarget.hpp"
#include "GLHeader.hpp"
#include "GLTexture2D.hpp"
#include <vector>
#include <memory>

namespace rhi {

class GLRenderTarget : public IRenderTarget {
public:
    ~GLRenderTarget();
    bool create(int width, int height) override;
    bool create(const FramebufferDesc& desc) override;
    bool attachCubeFace(ITexture3D* cube, int face, int mip) override;
    bool attachDepthCube(ITexture3D* cube, int mip) override;
    bool bind() override;
    bool unbind() override;
    void* colorTexture() override;
    ITexture2D* colorTexture2D(int attachment = 0) override;
    ITexture2D* depthTexture2D() override;
    bool resolveTo(IRenderTarget& dst) override;
    void* handle() override { return reinterpret_cast<void*>(static_cast<uintptr_t>(_fbo)); }
    void release() override;

private:
    GLuint _fbo{0}, _colorTex{0}, _rbo{0}, _depthTex{0}, _depthRbo{0};
    std::vector<GLuint> _colorTexs{};
    std::vector<GLuint> _colorRbos{};
    std::vector<std::unique_ptr<GLTexture2D>> _colorViews{};
    std::unique_ptr<GLTexture2D> _depthView{};
    int _width{0}, _height{0};
    bool _msaa{false};
    int _samples{0};
};

} // namespace rhi
