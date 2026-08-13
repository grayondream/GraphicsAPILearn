#pragma once
#include "VKHeader.hpp"
#include "base/Log.hpp"
#include <functional>

namespace rhi {

// Reuse a lazily-created transient command pool per texture for one-shot GPU
// uploads. Each upload allocates a fresh command buffer that is submitted,
// waited on, then destroyed here (RAII), so no reset is needed.
inline bool SubmitOneShot(vk::raii::Device& dev, vk::raii::Queue& queue, uint32_t family,
                          const std::function<void(vk::raii::CommandBuffer&)>& record) {
    vk::CommandPoolCreateInfo cpci(vk::CommandPoolCreateFlagBits::eTransient, family);
    auto cpr = dev.createCommandPool(cpci);
    if (cpr.result != vk::Result::eSuccess) return false;
    vk::raii::CommandPool pool(std::move(cpr.value));
    vk::CommandBufferAllocateInfo cba(*pool, vk::CommandBufferLevel::ePrimary, 1);
    auto cbr = dev.allocateCommandBuffers(cba);
    if (cbr.result != vk::Result::eSuccess) return false;
    vk::raii::CommandBuffer cmd(std::move(cbr.value[0]));

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    vk::Result br = cmd.begin(beginInfo);
    if (br != vk::Result::eSuccess) return false;
    record(cmd);
    vk::Result er = cmd.end();
    if (er != vk::Result::eSuccess) return false;

    vk::CommandBuffer cb = *cmd;
    vk::SubmitInfo si;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    vk::Result sr = queue.submit({si});
    if (sr != vk::Result::eSuccess) return false;
    return queue.waitIdle() == vk::Result::eSuccess;
}

inline bool TransitionImage(vk::raii::Device& dev, vk::raii::Queue& queue, uint32_t family,
                            vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                            vk::ImageAspectFlags aspect, uint32_t baseMip, uint32_t mipCount,
                            uint32_t baseLayer, uint32_t layerCount) {
    return SubmitOneShot(dev, queue, family, [&](vk::raii::CommandBuffer& cmd) {
        vk::ImageMemoryBarrier barrier{};
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
        barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspect;
        barrier.subresourceRange.baseMipLevel = baseMip;
        barrier.subresourceRange.levelCount = mipCount;
        barrier.subresourceRange.baseArrayLayer = baseLayer;
        barrier.subresourceRange.layerCount = layerCount;
        vk::PipelineStageFlags srcStage;
        vk::PipelineStageFlags dstStage;
        if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
            dstStage = vk::PipelineStageFlagBits::eTransfer;
        } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
                   newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            srcStage = vk::PipelineStageFlagBits::eTransfer;
            dstStage = vk::PipelineStageFlagBits::eFragmentShader;
        } else if (oldLayout == vk::ImageLayout::eUndefined &&
                   newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
            dstStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
        } else if (oldLayout == vk::ImageLayout::eUndefined &&
                   newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
            dstStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        } else {
            barrier.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
            barrier.dstAccessMask = vk::AccessFlagBits::eMemoryWrite;
            srcStage = vk::PipelineStageFlagBits::eAllCommands;
            dstStage = vk::PipelineStageFlagBits::eAllCommands;
        }
        cmd.pipelineBarrier(srcStage, dstStage, {}, {}, {}, {barrier});
    });
}

// Allocate a host-visible staging buffer of the given size and copy data in.
inline bool UploadStagingToImage(vk::raii::Device& dev, vk::raii::PhysicalDevice& phys,
                                 vk::raii::Queue& queue, uint32_t family,
                                 vk::Image image, const void* data, size_t size,
                                 vk::Extent3D extent, uint32_t baseArrayLayer, uint32_t layerCount,
                                 vk::ImageAspectFlags aspect, vk::ImageLayout finalLayout) {
    vk::BufferCreateInfo bci{};
    bci.size = size;
    bci.usage = vk::BufferUsageFlagBits::eTransferSrc;
    bci.sharingMode = vk::SharingMode::eExclusive;
    auto sbr = dev.createBuffer(bci);
    if (sbr.result != vk::Result::eSuccess) return false;
    vk::raii::Buffer staging(std::move(sbr.value));

    const vk::MemoryRequirements req = staging.getMemoryRequirements();
    const vk::PhysicalDeviceMemoryProperties props = phys.getMemoryProperties();
    const uint32_t memType = findMemoryType(props, req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    if (memType == UINT32_MAX) return false;
    vk::MemoryAllocateInfo mai(req.size, memType);
    auto sar = dev.allocateMemory(mai);
    if (sar.result != vk::Result::eSuccess) return false;
    vk::raii::DeviceMemory stagingMem(std::move(sar.value));

    vk::BindBufferMemoryInfo bbmi(*staging, *stagingMem, 0);
    if (dev.bindBufferMemory2({bbmi}) != vk::Result::eSuccess) return false;

    auto mp = stagingMem.mapMemory(0, size);
    if (mp.result != vk::Result::eSuccess) return false;
    std::memcpy(mp.value, data, size);
    stagingMem.unmapMemory();

    const bool ok = SubmitOneShot(dev, queue, family, [&](vk::raii::CommandBuffer& cmd) {
        vk::ImageMemoryBarrier toDst{};
        toDst.oldLayout = vk::ImageLayout::eUndefined;
        toDst.newLayout = vk::ImageLayout::eTransferDstOptimal;
        toDst.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
        toDst.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
        toDst.image = image;
        toDst.subresourceRange.aspectMask = aspect;
        toDst.subresourceRange.baseMipLevel = 0;
        toDst.subresourceRange.levelCount = 1;
        toDst.subresourceRange.baseArrayLayer = baseArrayLayer;
        toDst.subresourceRange.layerCount = layerCount;
        toDst.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
                            {}, {}, {}, {toDst});

        vk::BufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspect;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = baseArrayLayer;
        region.imageSubresource.layerCount = layerCount;
        region.imageExtent = extent;
        cmd.copyBufferToImage(*staging, image, vk::ImageLayout::eTransferDstOptimal, {region});

        vk::ImageMemoryBarrier toRead{};
        toRead.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        toRead.newLayout = finalLayout;
        toRead.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
        toRead.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
        toRead.image = image;
        toRead.subresourceRange.aspectMask = aspect;
        toRead.subresourceRange.baseMipLevel = 0;
        toRead.subresourceRange.levelCount = 1;
        toRead.subresourceRange.baseArrayLayer = baseArrayLayer;
        toRead.subresourceRange.layerCount = layerCount;
        toRead.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        toRead.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                            {}, {}, {}, {toRead});
    });
    return ok;
}

} // namespace rhi
