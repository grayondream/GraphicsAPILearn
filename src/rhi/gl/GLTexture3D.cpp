#include "GLTexture3D.hpp"

namespace rhi {

GLTexture3D::~GLTexture3D() { release(); }

bool GLTexture3D::init(const TextureDataView3D& data) {
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

void GLTexture3D::bind(unsigned int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_3D, _id);
}

void* GLTexture3D::handle() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(_id));
}

void GLTexture3D::release() {
    if (_id) { glDeleteTextures(1, &_id); _id = 0; }
}

} // namespace rhi
