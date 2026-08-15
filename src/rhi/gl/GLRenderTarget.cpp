#include "GLRenderTarget.hpp"
#include "GLTexture3D.hpp"
#include "rhi/core/ITexture3D.hpp"

namespace rhi {

GLRenderTarget::~GLRenderTarget() { release(); }

bool GLRenderTarget::create(int width, int height) {
    release();

    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);

    glGenTextures(1, &_colorTex);
    glBindTexture(GL_TEXTURE_2D, _colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _colorTex, 0);

    glGenRenderbuffers(1, &_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, _rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _rbo);

    const bool complete = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (!complete) {
        release();
        return false;
    }

    _width = width;
    _height = height;
    return true;
}

bool GLRenderTarget::create(const FramebufferDesc& desc) {
    if (desc.attachments.empty()) return create(desc.width, desc.height);

    release();
    _width = desc.width;
    _height = desc.height;
    _msaa = desc.samples > 0;
    _samples = desc.samples;

    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);

    std::vector<GLenum> drawBufs;
    for (size_t i = 0; i < desc.attachments.size(); ++i) {
        const auto& a = desc.attachments[i];
        if (a.type == AttachmentType::Color) {
            if (_msaa) {
                GLuint rb = 0;
                glGenRenderbuffers(1, &rb);
                glBindRenderbuffer(GL_RENDERBUFFER, rb);
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, _samples,
                                                 ToGLInternalFormat(a.format), _width, _height);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER, rb);
                _colorRbos.push_back(rb);
            } else {
                GLuint tex = 0;
                glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_2D, tex);
                glTexImage2D(GL_TEXTURE_2D, 0, ToGLInternalFormat(a.format), _width, _height, 0,
                             ToGLFormat(a.format, 0), ToGLSrcType(a.format), nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(a.minFilter));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ToGLMinFilter(a.magFilter));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ToGLWrap(a.wrapS));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ToGLWrap(a.wrapT));
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, tex, 0);
                _colorTexs.push_back(tex);
            }
            drawBufs.push_back(GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i));
        } else {
            const GLenum attach = (a.type == AttachmentType::DepthStencil)
                                      ? GL_DEPTH_STENCIL_ATTACHMENT
                                      : GL_DEPTH_ATTACHMENT;
            if (_msaa) {
                GLuint rb = 0;
                glGenRenderbuffers(1, &rb);
                glBindRenderbuffer(GL_RENDERBUFFER, rb);
                const GLenum fmt = (a.type == AttachmentType::DepthStencil)
                                       ? GL_DEPTH24_STENCIL8
                                       : ToGLInternalFormat(a.format);
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, _samples, fmt, _width, _height);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, attach, GL_RENDERBUFFER, rb);
                _depthRbo = rb;
            } else {
                GLuint tex = 0;
                glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_2D, tex);
                glTexImage2D(GL_TEXTURE_2D, 0, ToGLInternalFormat(a.format), _width, _height, 0,
                             ToGLFormat(a.format, 0), ToGLSrcType(a.format), nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(a.minFilter));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ToGLMinFilter(a.magFilter));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ToGLWrap(a.wrapS));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ToGLWrap(a.wrapT));
                if (a.wrapS == TextureWrap::ClampToBorder || a.wrapT == TextureWrap::ClampToBorder) {
                    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, a.borderColor);
                }
                glFramebufferTexture2D(GL_FRAMEBUFFER, attach, GL_TEXTURE_2D, tex, 0);
                _depthTex = tex;
            }
        }
    }

    if (!drawBufs.empty()) glDrawBuffers(static_cast<GLsizei>(drawBufs.size()), drawBufs.data());
    else { glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE); }

    const bool complete = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (!complete) {
        release();
        return false;
    }
    return true;
}

bool GLRenderTarget::attachCubeFace(ITexture3D* cube, int face, int mip) {
    if (!cube || face < 0 || face > 5 || !_fbo) return false;
    const bool isDepth = (static_cast<GLTexture3D*>(cube)->desc().format == TextureFormat::Depth32F);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    if (isDepth) {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                               static_cast<GLuint>(reinterpret_cast<uintptr_t>(cube->handle())), mip);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    } else {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                               static_cast<GLuint>(reinterpret_cast<uintptr_t>(cube->handle())), mip);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
    }
    return true;
}

bool GLRenderTarget::attachDepthCube(ITexture3D* cube, int mip) {
    if (!cube || mip < 0 || !_fbo) return false;
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                         static_cast<GLuint>(reinterpret_cast<uintptr_t>(cube->handle())), mip);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    return true;
}

bool GLRenderTarget::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    return true;
}

bool GLRenderTarget::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void* GLRenderTarget::colorTexture() {
    return _colorTexs.empty()
               ? reinterpret_cast<void*>(static_cast<uintptr_t>(_colorTex))
               : reinterpret_cast<void*>(static_cast<uintptr_t>(_colorTexs[0]));
}

ITexture2D* GLRenderTarget::colorTexture2D(int attachment) {
    if (attachment < 0) return nullptr;
    GLuint tex = 0;
    if (!_colorTexs.empty()) {
        if (static_cast<size_t>(attachment) >= _colorTexs.size()) return nullptr;
        tex = _colorTexs[attachment];
    } else {
        if (attachment != 0) return nullptr;
        tex = _colorTex;
    }
    if (!tex) return nullptr;
    if (static_cast<size_t>(attachment) >= _colorViews.size())
        _colorViews.resize(static_cast<size_t>(attachment) + 1);
    if (!_colorViews[attachment]) {
        auto v = std::make_unique<GLTexture2D>();
        v->adopt(tex);
        _colorViews[attachment] = std::move(v);
    }
    return _colorViews[attachment].get();
}

ITexture2D* GLRenderTarget::depthTexture2D() {
    if (!_depthTex) return nullptr;
    if (!_depthView) {
        auto v = std::make_unique<GLTexture2D>();
        v->adopt(_depthTex);
        _depthView = std::move(v);
    }
    return _depthView.get();
}

bool GLRenderTarget::resolveTo(IRenderTarget& dst) {
    if (!_fbo) return false;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                      static_cast<GLuint>(reinterpret_cast<uintptr_t>(dst.handle())));
    glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void GLRenderTarget::release() {
    for (auto rb : _colorRbos) if (rb) glDeleteRenderbuffers(1, &rb);
    _colorRbos.clear();
    if (_depthRbo) { glDeleteRenderbuffers(1, &_depthRbo); _depthRbo = 0; }
    for (auto tex : _colorTexs) if (tex) glDeleteTextures(1, &tex);
    _colorTexs.clear();
    if (_depthTex) { glDeleteTextures(1, &_depthTex); _depthTex = 0; }
    if (_rbo) { glDeleteRenderbuffers(1, &_rbo); _rbo = 0; }
    if (_colorTex) { glDeleteTextures(1, &_colorTex); _colorTex = 0; }
    if (_fbo) { glDeleteFramebuffers(1, &_fbo); _fbo = 0; }
    _colorViews.clear();
    _depthView.reset();
    _width = 0;
    _height = 0;
    _msaa = false;
    _samples = 0;
}

} // namespace rhi
