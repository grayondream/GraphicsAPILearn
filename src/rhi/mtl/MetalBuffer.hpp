#pragma once

#include "rhi/core/IBuffer.hpp"
#include <cstdint>
#include <cstring>

#if defined(__APPLE__)

#import <Metal/Metal.h>

namespace rhi::mtl {

class MetalBuffer : public IBuffer {
public:
    explicit MetalBuffer(void* device);
    ~MetalBuffer() override;

    bool init(const void* data, size_t size, BufferType type) override;
    bool update(const void* data, size_t size, size_t offset) override;
    bool bindRange(uint32_t binding, size_t offset, size_t size) override;
    bool bind() override;
    void* handle() override;

    uint32_t binding() const { return _binding; }
    size_t bindOffset() const { return _bindOffset; }
    size_t bindSize() const { return _bindSize; }
    BufferType type() const { return _type; }

private:
    id<MTLDevice> _device{nil};
    id<MTLBuffer> _buffer{nil};
    BufferType _type{BufferType::Vertex};
    size_t _size{0};
    uint32_t _binding{0};
    size_t _bindOffset{0};
    size_t _bindSize{0};
};

} // namespace rhi::mtl

#else // non-Apple fallback

namespace rhi::mtl {

class MetalBuffer : public IBuffer {
public:
    MetalBuffer() = default;
    ~MetalBuffer() override = default;

    bool init(const void*, size_t, BufferType) override { return false; }
    bool update(const void*, size_t, size_t) override { return false; }
    bool bindRange(uint32_t, size_t, size_t) override { return false; }
    bool bind() override { return false; }
    void* handle() override { return nullptr; }
};

} // namespace rhi::mtl

#endif
