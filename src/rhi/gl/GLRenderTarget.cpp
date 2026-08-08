#include "GLRenderTarget.hpp"

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

bool GLRenderTarget::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    return true;
}

bool GLRenderTarget::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void* GLRenderTarget::colorTexture() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(_colorTex));
}

void GLRenderTarget::release() {
    if (_rbo) { glDeleteRenderbuffers(1, &_rbo); _rbo = 0; }
    if (_colorTex) { glDeleteTextures(1, &_colorTex); _colorTex = 0; }
    if (_fbo) { glDeleteFramebuffers(1, &_fbo); _fbo = 0; }
    _width = 0;
    _height = 0;
}

} // namespace rhi
