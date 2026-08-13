#include "VKBuffer.hpp"
#include "base/Log.hpp"
#include <cstring>

namespace rhi {

VKBuffer::VKBuffer(vk::raii::Device& device, vk::raii::PhysicalDevice& phys,
                   vk::raii::Queue& queue, uint32_t graphicsFamily)
    : _dev(device), _phys(phys), _queue(queue), _graphicsFamily(graphicsFamily) {}

bool VKBuffer::init(const void* data, size_t size, BufferType type) {
    _type = type;
    _size = size;
    const bool uniform = (type == BufferType::Uniform);

    vk::BufferUsageFlags usage = uniform ? vk::BufferUsageFlagBits::eUniformBuffer
        : ((type == BufferType::Index) ? vk::BufferUsageFlagBits::eIndexBuffer
                                       : vk::BufferUsageFlagBits::eVertexBuffer);
    if (!uniform) usage |= vk::BufferUsageFlagBits::eTransferDst;

    vk::BufferCreateInfo bci{};
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = vk::SharingMode::eExclusive;
    auto br = _dev.createBuffer(bci);
    if (br.result != vk::Result::eSuccess) {
        LOGE("VKBuffer: createBuffer failed");
        return false;
    }
    _buffer = std::move(br.value);

    const vk::MemoryRequirements req = _buffer.getMemoryRequirements();
    const vk::PhysicalDeviceMemoryProperties props = _phys.getMemoryProperties();
    _memProps = uniform ? (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
                        : vk::MemoryPropertyFlagBits::eDeviceLocal;
    const uint32_t memType = findMemoryType(props, req.memoryTypeBits, _memProps);
    if (memType == UINT32_MAX) {
        LOGE("VKBuffer: no suitable memory type");
        return false;
    }
    vk::MemoryAllocateInfo mai(req.size, memType);
    auto ar = _dev.allocateMemory(mai);
    if (ar.result != vk::Result::eSuccess) {
        LOGE("VKBuffer: allocateMemory failed");
        return false;
    }
    _memory = std::move(ar.value);

    vk::BindBufferMemoryInfo bbmi(*_buffer, *_memory, 0);
    vk::Result bb = _dev.bindBufferMemory2({bbmi});
    if (bb != vk::Result::eSuccess) {
        LOGE("VKBuffer: bindBufferMemory failed");
        return false;
    }

    if (data) {
        if (uniform) {
            auto mp = _memory.mapMemory(0, size);
            if (mp.result != vk::Result::eSuccess) return false;
            std::memcpy(mp.value, data, size);
            _memory.unmapMemory();
        } else if (!uploadStaging(data, size, 0)) {
            return false;
        }
    }

    if (uniform && _notifier) _notifier->onUniformCreated(this);
    return true;
}

bool VKBuffer::update(const void* data, size_t size, size_t offset) {
    if (_buffer == nullptr || _memory == nullptr || !data) return false;
    if (_memProps & vk::MemoryPropertyFlagBits::eHostVisible) {
        auto mp = _memory.mapMemory(offset, size);
        if (mp.result != vk::Result::eSuccess) return false;
        std::memcpy(mp.value, data, size);
        if (!(_memProps & vk::MemoryPropertyFlagBits::eHostCoherent)) {
            vk::MappedMemoryRange range(*_memory, offset, size);
            _dev.flushMappedMemoryRanges({range});
        }
        _memory.unmapMemory();
    } else if (!uploadStaging(data, size, offset)) {
        return false;
    }
    if (_type == BufferType::Uniform && _notifier) _notifier->onUniformUpdated(this);
    return true;
}

bool VKBuffer::uploadStaging(const void* data, size_t size, size_t offset) {
    vk::BufferCreateInfo bci{};
    bci.size = size;
    bci.usage = vk::BufferUsageFlagBits::eTransferSrc;
    bci.sharingMode = vk::SharingMode::eExclusive;
    auto sbr = _dev.createBuffer(bci);
    if (sbr.result != vk::Result::eSuccess) {
        LOGE("VKBuffer: staging createBuffer failed");
        return false;
    }
    vk::raii::Buffer staging(std::move(sbr.value));

    const vk::MemoryRequirements req = staging.getMemoryRequirements();
    const vk::PhysicalDeviceMemoryProperties props = _phys.getMemoryProperties();
    const uint32_t memType = findMemoryType(props, req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    if (memType == UINT32_MAX) {
        LOGE("VKBuffer: staging no memory type");
        return false;
    }
    vk::MemoryAllocateInfo mai(req.size, memType);
    auto sar = _dev.allocateMemory(mai);
    if (sar.result != vk::Result::eSuccess) {
        LOGE("VKBuffer: staging allocateMemory failed");
        return false;
    }
    vk::raii::DeviceMemory stagingMem(std::move(sar.value));

    vk::BindBufferMemoryInfo bbmi(*staging, *stagingMem, 0);
    vk::Result bb = _dev.bindBufferMemory2({bbmi});
    if (bb != vk::Result::eSuccess) return false;

    auto mp = stagingMem.mapMemory(0, size);
    if (mp.result != vk::Result::eSuccess) return false;
    std::memcpy(mp.value, data, size);
    stagingMem.unmapMemory();

    vk::CommandPoolCreateInfo cpci(vk::CommandPoolCreateFlagBits::eTransient, _graphicsFamily);
    auto cpr = _dev.createCommandPool(cpci);
    if (cpr.result != vk::Result::eSuccess) return false;
    _stagingPool = std::move(cpr.value);
    vk::CommandBufferAllocateInfo cba(*_stagingPool, vk::CommandBufferLevel::ePrimary, 1);
    auto cbr = _dev.allocateCommandBuffers(cba);
    if (cbr.result != vk::Result::eSuccess) return false;
    vk::raii::CommandBuffer cmd(std::move(cbr.value[0]));

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    vk::Result br = cmd.begin(beginInfo);
    if (br != vk::Result::eSuccess) return false;
    vk::BufferCopy copyRegion(0, offset, size);
    cmd.copyBuffer(*staging, *_buffer, {copyRegion});
    vk::Result er = cmd.end();
    if (er != vk::Result::eSuccess) return false;

    vk::CommandBuffer cb = *cmd;
    vk::SubmitInfo si;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    vk::Result sr = _queue.submit({si});
    if (sr != vk::Result::eSuccess) return false;
    vk::Result wr = _queue.waitIdle();
    if (wr != vk::Result::eSuccess) return false;
    return true;
}

bool VKBuffer::bindRange(uint32_t, size_t, size_t) { return true; }

bool VKBuffer::bind() { return true; }

void* VKBuffer::handle() {
    return reinterpret_cast<void*>(static_cast<VkBuffer>(*_buffer));
}
} // namespace rhi