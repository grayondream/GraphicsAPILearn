#include "GLTexture2D.hpp"

namespace rhi {

GLTexture2D::~GLTexture2D() { release(); }

bool GLTexture2D::init(const TextureDataView2D& data) {
    if (!_id) glGenTextures(1, &_id);
    glBindTexture(GL_TEXTURE_2D, _id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLenum fmt = (data.channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, data.width, data.height, 0, fmt, GL_UNSIGNED_BYTE, data.data);
    glGenerateMipmap(GL_TEXTURE_2D);
    return true;
}

bool GLTexture2D::init(const TextureDesc& desc, const TextureDataView2D& data) {
    _desc = desc;
    if (!_id) glGenTextures(1, &_id);
    glBindTexture(GL_TEXTURE_2D, _id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ToGLWrap(desc.wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ToGLWrap(desc.wrapT));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(desc.minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    (desc.magFilter == TextureFilter::Nearest) ? GL_NEAREST : GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, ToGLInternalFormat(desc.format),
                 data.width, data.height, 0, ToGLFormat(desc.format, data.channels),
                 ToGLSrcType(desc.format), data.data);
    if (desc.generateMipmap) glGenerateMipmap(GL_TEXTURE_2D);
    return true;
}

bool GLTexture2D::createEmpty(const TextureDesc& desc, int width, int height) {
    _desc = desc;
    if (!_id) glGenTextures(1, &_id);
    glBindTexture(GL_TEXTURE_2D, _id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ToGLWrap(desc.wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ToGLWrap(desc.wrapT));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(desc.minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    (desc.magFilter == TextureFilter::Nearest) ? GL_NEAREST : GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, ToGLInternalFormat(desc.format),
                 width, height, 0, ToGLFormat(desc.format, 0), ToGLSrcType(desc.format), nullptr);
    return true;
}

void GLTexture2D::bind(unsigned int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, _id);
}

void* GLTexture2D::handle() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(_id));
}

void GLTexture2D::adopt(GLuint id) {
    if (_id && _owns) glDeleteTextures(1, &_id);
    _id = id;
    _owns = false;
}

void GLTexture2D::release() {
    if (_id && _owns) glDeleteTextures(1, &_id);
    _id = 0;
    _owns = true;
}

} // namespace rhi
