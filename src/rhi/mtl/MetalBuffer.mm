#if defined(__APPLE__)

#import <Metal/Metal.h>
#include "MetalBuffer.hpp"

namespace rhi::mtl {

MetalBuffer::MetalBuffer(void* device)
    : _device((__bridge id<MTLDevice>)device) {}

MetalBuffer::~MetalBuffer() {
    _buffer = nil;
    _device = nil;
}

bool MetalBuffer::init(const void* data, size_t size, BufferType type) {
    if (!_device || size == 0) return false;

    _type = type;
    _size = size;

    MTLResourceOptions options = MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined;

    if (data) {
        _buffer = [_device newBufferWithBytes:data
                                       length:size
                                      options:options];
    } else {
        _buffer = [_device newBufferWithLength:size
                                       options:options];
    }

    return _buffer != nil;
}

bool MetalBuffer::update(const void* data, size_t size, size_t offset) {
    if (!_buffer || !data || size == 0) return false;
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
