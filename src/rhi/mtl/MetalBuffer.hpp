#pragma once

#include "rhi/core/IBuffer.hpp"
#include <cstdint>
#include <cstring>
#include <functional>

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

    // Ring buffer: returns the byte offset within the MTLBuffer where the
    // most recent update() wrote its data.  prepareDraw() uses this so that
    // each draw call binds the correct slot even when multiple draws share
    // the same uniform buffer.
    size_t submittedOffset() const { return _submittedBase; }

    void setBindCallback(std::function<void(MetalBuffer*, uint32_t)> cb) { _bindCb = std::move(cb); }

private:
    void* _device{nullptr};
    id<MTLBuffer> __strong _buffer{nil};
    BufferType _type{BufferType::Vertex};
    size_t _size{0};           // logical size (one slot)
    uint32_t _binding{0};
    size_t _bindOffset{0};
    size_t _bindSize{0};
    std::function<void(MetalBuffer*, uint32_t)> _bindCb{};

    // Ring buffer state for uniform buffers
    static constexpr size_t kRingSlots = 256;
    size_t _slotSize{0};       // one slot's byte count (uniform only)
    uint32_t _ringHead{0};     // monotonically increasing update counter
    size_t _submittedBase{0};  // byte offset of the last-written slot
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
