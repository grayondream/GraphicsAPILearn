#include "GLTexture3D.hpp"

namespace rhi {

GLTexture3D::~GLTexture3D() { release(); }

bool GLTexture3D::init(const TextureDataView3D& data) {
    _target = GL_TEXTURE_3D;
    if (!_id) glGenTextures(1, &_id);
    glBindTexture(GL_TEXTURE_3D, _id);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLenum fmt = (data.channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage3D(GL_TEXTURE_3D, 0, fmt, data.width, data.height, data.depth, 0, fmt, GL_UNSIGNED_BYTE, data.data);
    glGenerateMipmap(GL_TEXTURE_3D);
    return true;
}

bool GLTexture3D::initCube(const TextureDesc& desc, const TextureDataView2D* faces) {
    _desc = desc;
    _target = GL_TEXTURE_CUBE_MAP;
    if (!_id) glGenTextures(1, &_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, _id);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, ToGLWrap(desc.wrapS));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, ToGLWrap(desc.wrapT));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, ToGLWrap(desc.wrapR));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(desc.minFilter));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER,
                    (desc.magFilter == TextureFilter::Nearest) ? GL_NEAREST : GL_LINEAR);
    for (int i = 0; i < 6; ++i) {
        const auto& f = faces[i];
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, ToGLInternalFormat(desc.format),
                     f.width, f.height, 0, ToGLFormat(desc.format, f.channels),
                     ToGLSrcType(desc.format), f.data);
    }
    if (desc.generateMipmap) glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    return true;
}

bool GLTexture3D::createEmpty(const TextureDesc& desc, int width, int height) {
    _desc = desc;
    _target = GL_TEXTURE_CUBE_MAP;
    if (!_id) glGenTextures(1, &_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, _id);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, ToGLWrap(desc.wrapS));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, ToGLWrap(desc.wrapT));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, ToGLWrap(desc.wrapR));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(desc.minFilter));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER,
                    (desc.magFilter == TextureFilter::Nearest) ? GL_NEAREST : GL_LINEAR);
    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, ToGLInternalFormat(desc.format),
                     width, height, 0, ToGLFormat(desc.format, 0), ToGLSrcType(desc.format), nullptr);
    }
    if (desc.generateMipmap) glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    return true;
}

void GLTexture3D::bind(unsigned int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(_target, _id);
}

void* GLTexture3D::handle() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(_id));
}

void GLTexture3D::release() {
    if (_id) { glDeleteTextures(1, &_id); _id = 0; }
}

void GLTexture3D::genCubeMipmaps() {
    if (_id && _target == GL_TEXTURE_CUBE_MAP) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, _id);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    }
}

} // namespace rhi
