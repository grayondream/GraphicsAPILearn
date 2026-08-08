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

void GLTexture2D::bind(unsigned int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, _id);
}

void* GLTexture2D::handle() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(_id));
}

void GLTexture2D::release() {
    if (_id) { glDeleteTextures(1, &_id); _id = 0; }
}

} // namespace rhi
