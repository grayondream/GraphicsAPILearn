#pragma once
#include "Common.hpp"
#include <cstdint>

namespace rhi {

enum class BufferType : uint8_t { Vertex, Index, Uniform };

class IBuffer {
public:
    virtual ~IBuffer() = default;
    virtual bool init(const void* data, size_t size, BufferType type) = 0;
    virtual bool update(const void* data, size_t size, size_t offset = 0) = 0;  // 新增
    virtual bool bindRange(uint32_t binding, size_t offset, size_t size) = 0;    // 新增
    virtual bool bind() = 0;
    virtual void* handle() = 0;
};

} // namespace rhi
