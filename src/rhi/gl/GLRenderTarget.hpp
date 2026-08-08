#pragma once
#include "rhi/core/IRenderTarget.hpp"
#include "GLHeader.hpp"

namespace rhi {

class GLRenderTarget : public IRenderTarget {
public:
    ~GLRenderTarget();
    bool create(int width, int height) override;
    bool bind() override;
    bool unbind() override;
    void* colorTexture() override;
    void* handle() override { return reinterpret_cast<void*>(static_cast<uintptr_t>(_fbo)); }
    void release() override;

private:
    GLuint _fbo{0}, _colorTex{0}, _rbo{0};
    int _width{0}, _height{0};
};

} // namespace rhi
