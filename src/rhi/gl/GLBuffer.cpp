#include "GLBuffer.hpp"

namespace rhi {

GLBuffer::~GLBuffer() {
    if (_id) glDeleteBuffers(1, &_id);
}

bool GLBuffer::init(const void* data, size_t size, BufferType type) {
    _type = type;
    if (!_id) glGenBuffers(1, &_id);
    GLenum target = (_type == BufferType::Index) ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    glBindBuffer(target, _id);
    glBufferData(target, size, data, GL_STATIC_DRAW);
    return true;
}

bool GLBuffer::bind() {
    GLenum target = (_type == BufferType::Index) ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    glBindBuffer(target, _id);
    return true;
}

void* GLBuffer::handle() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(_id));
}

} // namespace rhi
