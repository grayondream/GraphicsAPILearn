#include "GLBuffer.hpp"

namespace rhi {

GLBuffer::~GLBuffer() {
    if (_id) glDeleteBuffers(1, &_id);
}

GLenum GLBuffer::targetFor() const {
    switch (_type) {
        case BufferType::Index:   return GL_ELEMENT_ARRAY_BUFFER;
        case BufferType::Uniform: return GL_UNIFORM_BUFFER;
        case BufferType::Vertex:  break;
    }
    return GL_ARRAY_BUFFER;
}

bool GLBuffer::init(const void* data, size_t size, BufferType type) {
    _type = type;
    _size = size;
    if (!_id) glGenBuffers(1, &_id);
    glBindBuffer(targetFor(), _id);
    glBufferData(targetFor(), size, data, GL_STATIC_DRAW);
    return true;
}

bool GLBuffer::update(const void* data, size_t size, size_t offset) {
    if (!_id) return false;
    glBindBuffer(targetFor(), _id);
    glBufferSubData(targetFor(), static_cast<GLintptr>(offset), size, data);
    return true;
}

bool GLBuffer::bindRange(uint32_t binding, size_t offset, size_t size) {
    if (!_id) return false;
    glBindBufferRange(GL_UNIFORM_BUFFER, binding, _id,
                      static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
    return true;
}

bool GLBuffer::bind() {
    if (!_id) return false;
    glBindBuffer(targetFor(), _id);
    return true;
}

void* GLBuffer::handle() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(_id));
}

} // namespace rhi
