#pragma once
#include "rhi/core/IBuffer.hpp"
#include "GLHeader.hpp"
#include <cstdint>

namespace rhi {

class GLBuffer : public IBuffer {
public:
    ~GLBuffer();
    bool init(const void* data, size_t size, BufferType type) override;
    bool bind() override;
    void* handle() override;

private:
    GLuint _id{0};
    BufferType _type{BufferType::Vertex};
};

} // namespace rhi
