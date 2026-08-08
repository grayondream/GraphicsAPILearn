#pragma once
#include "Common.hpp"
#include <cstdint>

namespace rhi {

enum class BufferType : uint8_t { Vertex, Index };

class IBuffer {
public:
    virtual ~IBuffer() = default;
    virtual bool init(const void* data, size_t size, BufferType type) = 0;
    virtual bool bind() = 0;
    virtual void* handle() = 0;
};

} // namespace rhi
