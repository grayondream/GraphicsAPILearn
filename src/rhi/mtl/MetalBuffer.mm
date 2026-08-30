#if defined(__APPLE__)

#import <Metal/Metal.h>
#include "MetalBuffer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <glm/gtc/type_ptr.hpp>

namespace rhi::mtl {

MetalBuffer::MetalBuffer(void* device)
    : _device(device) {}

MetalBuffer::~MetalBuffer() {
    _buffer = nil;
    _device = nullptr;
}

bool MetalBuffer::init(const void* data, size_t size, BufferType type) {
    if (!_device || size == 0) return false;

    id<MTLDevice> device = (__bridge id<MTLDevice>)_device;
    _type = type;
    _size = size;

    // Uniform buffers use a ring buffer: each update() writes to the next slot
    // so that multiple draws sharing the same buffer don't overwrite each other.
    const bool uniform = (type == BufferType::Uniform);
    const size_t allocSize = uniform ? size * kRingSlots : size;
    if (uniform) _slotSize = size;

    MTLResourceOptions options = MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined;

    if (data && !uniform) {
        _buffer = [device newBufferWithBytes:data
                                       length:allocSize
                                      options:options];
    } else {
        _buffer = [device newBufferWithLength:allocSize
                                       options:options];
    }

    return _buffer != nil;
}

bool MetalBuffer::update(const void* data, size_t size, size_t offset) {
    if (!_buffer || !data || size == 0) return false;

    // Ring buffer path for uniform buffers: each call writes to the next slot
    // so that multiple draws per frame don't overwrite each other's data.
    if (_type == BufferType::Uniform && _slotSize > 0) {
        _ringHead++;
        uint32_t slot = _ringHead % kRingSlots;
        size_t dstOffset = slot * _slotSize + offset;
        if (dstOffset + size > _size * kRingSlots) return false;

        void* contents = [_buffer contents];
        if (!contents) return false;

        auto* dst = static_cast<uint8_t*>(contents) + dstOffset;
        memcpy(dst, data, size);

        // Convert GL NDC z [-1,1] → Metal [0,1]: z' = 0.5*z + 0.5*w
        if (size >= sizeof(UniformBlock)) {
            float* proj = glm::value_ptr(reinterpret_cast<UniformBlock*>(dst)->projection);
            for (int i = 0; i < 4; ++i) {
                int z = i * 4 + 2;
                int w = i * 4 + 3;
                proj[z] = 0.5f * proj[z] + 0.5f * proj[w];
            }
        }

        _submittedBase = slot * _slotSize;
        return true;
    }

    // Non-uniform path: direct write at the requested offset
    if (offset + size > _size) return false;
    void* contents = [_buffer contents];
    if (!contents) return false;
    memcpy(static_cast<uint8_t*>(contents) + offset, data, size);
    return true;
}

bool MetalBuffer::bindRange(uint32_t binding, size_t offset, size_t size) {
    _binding = binding;
    _bindOffset = offset;
    _bindSize = size;
    if (_bindCb) _bindCb(this, binding);
    return true;
}

bool MetalBuffer::bind() {
    return _buffer != nil;
}

void* MetalBuffer::handle() {
    return (__bridge void*)_buffer;
}

} // namespace rhi::mtl

#endif
