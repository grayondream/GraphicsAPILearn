#pragma once
#include "rhi/core/IBuffer.hpp"
#include "GLHeader.hpp"
#include <cstdint>

namespace rhi {

class GLBuffer : public IBuffer {
public:
    ~GLBuffer();
    bool init(const void* data, size_t size, BufferType type) override;
    bool update(const void* data, size_t size, size_t offset) override;
    bool bindRange(uint32_t binding, size_t offset, size_t size) override;
    bool bind() override;
    void* handle() override;

private:
    GLenum targetFor() const;
    GLuint _id{0};
    BufferType _type{BufferType::Vertex};
    size_t _size{0};
};

} // namespace rhi
