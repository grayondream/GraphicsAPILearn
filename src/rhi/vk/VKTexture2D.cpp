#include "VKTexture2D.hpp"
#include "VKFormat.hpp"
#include "VKUpload.hpp"
#include "base/Log.hpp"
#include <cstring>

namespace rhi {

bool VKTexture2D::init(const TextureDataView2D& data) {
    return init(TextureDesc{}, data);
}

bool VKTexture2D::init(const TextureDesc& desc, const TextureDataView2D& data) {
    release();
    _format = ToVkTextureFormat(desc.format);
    _extent = vk::Extent2D(static_cast<uint32_t>(data.width), static_cast<uint32_t>(data.height));
    _minFilter = desc.minFilter;
    _mipLevels = desc.generateMipmap ? ComputeMipLevels(data.width, data.height) : 1;

    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
    if (_mipLevels > 1) usage |= vk::ImageUsageFlagBits::eTransferSrc;
    if (!createImage(desc, data.width, data.height, _mipLevels, usage)) return false;

    const size_t size = static_cast<size_t>(data.width) * static_cast<size_t>(data.height) *
                        ToVkTexelSize(desc.format);
    if (!UploadStagingToImage(_dev, _phys, _queue, _graphicsFamily, *_image, data.data, size,
                              vk::Extent3D(_extent.width, _extent.height, 1), 0, 1,
                              vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eShaderReadOnlyOptimal)) {
        return false;
    }
    if (_mipLevels > 1) {
        if (!genMipmaps(data.width, data.height)) return false;
        _layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }
    if (!createSampler(desc)) return false;
    if (!createView(vk::ImageAspectFlagBits::eColor)) return false;
    _valid = true;
    return true;
}

bool VKTexture2D::createEmpty(const TextureDesc& desc, int width, int height) {
    release();
    _format = ToVkTextureFormat(desc.format);
    _extent = vk::Extent2D(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    _minFilter = desc.minFilter;
    _mipLevels = 1;

    const bool depth = ToVkAspect(desc.format) != vk::ImageAspectFlagBits::eColor;
    const vk::ImageAspectFlags aspect = ToVkAspect(desc.format);
    vk::ImageUsageFlags usage = depth ? vk::ImageUsageFlagBits::eDepthStencilAttachment
                                      : (vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
                                         vk::ImageUsageFlagBits::eTransferDst);
    if (!createImage(desc, width, height, 1, usage)) return false;

    if (!depth) {
        const vk::ImageLayout finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        if (!TransitionImage(_dev, _queue, _graphicsFamily, *_image, vk::ImageLayout::eUndefined,
                             finalLayout, aspect, 0, 1, 0, 1)) {
            return false;
        }
        _layout = finalLayout;
    }
    if (!createSampler(desc)) return false;
    if (!createView(aspect)) return false;
    _valid = true;
    return true;
}

bool VKTexture2D::adopt(vk::ImageView view, vk::Format format, vk::Extent2D ext, const TextureDesc& desc) {
    release();
    _format = format;
    _extent = ext;
    _mipLevels = 1;
    _adopted = true;
    _adoptedView = view;

    vk::SamplerCreateInfo sci{};
    sci.magFilter = ToVkFilter(desc.magFilter);
    sci.minFilter = ToVkFilter(desc.minFilter);
    sci.mipmapMode = vk::SamplerMipmapMode::eNearest;
    sci.addressModeU = ToVkWrap(desc.wrapS);
    sci.addressModeV = ToVkWrap(desc.wrapT);
    sci.addressModeW = ToVkWrap(desc.wrapR);
    sci.minLod = 0.0f;
    sci.maxLod = 1.0f;
    if (desc.wrapS == TextureWrap::ClampToBorder || desc.wrapT == TextureWrap::ClampToBorder ||
        desc.wrapR == TextureWrap::ClampToBorder) {
        // 深度纹理（如 shadow map）用 CLAMP_TO_BORDER，边框色=far（1.0）时超范围
        // 采样不产生阴影；默认黑色边框会把超范围判成近深度导致大片假阴影。
        sci.borderColor = vk::BorderColor::eFloatOpaqueWhite;
    }
    auto sr = _dev.createSampler(sci);
    if (sr.result != vk::Result::eSuccess) return false;
    _sampler = std::move(sr.value);

    _valid = true;
    return true;
}

bool VKTexture2D::createImage(const TextureDesc& desc, int width, int height, uint32_t mipLevels,
                              vk::ImageUsageFlags usage) {
    vk::ImageCreateInfo ici{};
    ici.flags = desc.multisample ? vk::ImageCreateFlagBits::eMutableFormat : vk::ImageCreateFlagBits{};
    ici.imageType = vk::ImageType::e2D;
    ici.format = _format;
    ici.extent = vk::Extent3D(static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1);
    ici.mipLevels = mipLevels;
    ici.arrayLayers = 1;
    ici.samples = desc.multisample ? ToVkSamples(desc.samples) : vk::SampleCountFlagBits::e1;
    ici.tiling = vk::ImageTiling::eOptimal;
    ici.usage = usage;
    ici.sharingMode = vk::SharingMode::eExclusive;
    ici.initialLayout = vk::ImageLayout::eUndefined;
    auto ir = _dev.createImage(ici);
    if (ir.result != vk::Result::eSuccess) {
        LOGE("VKTexture2D: createImage failed");
        return false;
    }
    _image = std::move(ir.value);

    const vk::MemoryRequirements req = _image.getMemoryRequirements();
    const vk::PhysicalDeviceMemoryProperties props = _phys.getMemoryProperties();
    const uint32_t memType = findMemoryType(props, req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    if (memType == UINT32_MAX) {
        LOGE("VKTexture2D: no device-local memory type");
        return false;
    }
    vk::MemoryAllocateInfo mai(req.size, memType);
    auto ar = _dev.allocateMemory(mai);
    if (ar.result != vk::Result::eSuccess) {
        LOGE("VKTexture2D: allocateMemory failed");
        return false;
    }
    _memory = std::move(ar.value);
    vk::BindImageMemoryInfo bimi(*_image, *_memory, 0);
    return _dev.bindImageMemory2({bimi}) == vk::Result::eSuccess;
}

bool VKTexture2D::createSampler(const TextureDesc& desc) {
    vk::SamplerCreateInfo sci{};
    sci.magFilter = ToVkFilter(desc.magFilter);
    sci.minFilter = ToVkFilter(desc.minFilter);
    sci.mipmapMode = ToVkMipFilter(desc.minFilter);
    sci.addressModeU = ToVkWrap(desc.wrapS);
    sci.addressModeV = ToVkWrap(desc.wrapT);
    sci.addressModeW = ToVkWrap(desc.wrapR);
    sci.mipLodBias = 0.0f;
    sci.anisotropyEnable = vk::False;
    sci.compareEnable = vk::False;
    sci.minLod = 0.0f;
    sci.maxLod = static_cast<float>(_mipLevels);
    auto sr = _dev.createSampler(sci);
    if (sr.result != vk::Result::eSuccess) {
        LOGE("VKTexture2D: createSampler failed");
        return false;
    }
    _sampler = std::move(sr.value);
    return true;
}

bool VKTexture2D::createView(vk::ImageAspectFlags aspect) {
    vk::ImageViewCreateInfo vci{};
    vci.image = *_image;
    vci.viewType = vk::ImageViewType::e2D;
    vci.format = _format;
    vci.components.r = vk::ComponentSwizzle::eIdentity;
    vci.components.g = vk::ComponentSwizzle::eIdentity;
    vci.components.b = vk::ComponentSwizzle::eIdentity;
    vci.components.a = vk::ComponentSwizzle::eIdentity;
    vci.subresourceRange.aspectMask = aspect;
    vci.subresourceRange.baseMipLevel = 0;
    vci.subresourceRange.levelCount = _mipLevels;
    vci.subresourceRange.baseArrayLayer = 0;
    vci.subresourceRange.layerCount = 1;
    auto vr = _dev.createImageView(vci);
    if (vr.result != vk::Result::eSuccess) {
        LOGE("VKTexture2D: createImageView failed");
        return false;
    }
    _view = std::move(vr.value);
    return true;
}

bool VKTexture2D::genMipmaps(int width, int height) {
    const vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor;
    vk::ImageLayout cur = vk::ImageLayout::eShaderReadOnlyOptimal;
    if (!TransitionImage(_dev, _queue, _graphicsFamily, *_image, cur, vk::ImageLayout::eTransferSrcOptimal,
                         aspect, 0, _mipLevels, 0, 1)) {
        return false;
    }
    const bool ok = SubmitOneShot(_dev, _queue, _graphicsFamily, [&](vk::raii::CommandBuffer& cmd) {
        int32_t w = width;
        int32_t h = height;
        for (uint32_t mip = 1; mip < _mipLevels; mip++) {
            vk::ImageMemoryBarrier barrier{};
            barrier.oldLayout = vk::ImageLayout::eUndefined;
            barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
            barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
            barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
            barrier.image = *_image;
            barrier.subresourceRange.aspectMask = aspect;
            barrier.subresourceRange.baseMipLevel = mip;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
                                {}, {}, {}, {barrier});

            vk::ImageBlit blit{};
            blit.srcSubresource.aspectMask = aspect;
            blit.srcSubresource.mipLevel = mip - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.srcOffsets[0] = vk::Offset3D(0, 0, 0);
            blit.srcOffsets[1] = vk::Offset3D(w, h, 1);
            blit.dstSubresource.aspectMask = aspect;
            blit.dstSubresource.mipLevel = mip;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;
            blit.dstOffsets[0] = vk::Offset3D(0, 0, 0);
            blit.dstOffsets[1] = vk::Offset3D(w > 1 ? w / 2 : 1, h > 1 ? h / 2 : 1, 1);
            cmd.blitImage(*_image, vk::ImageLayout::eTransferSrcOptimal,
                          *_image, vk::ImageLayout::eTransferDstOptimal, {blit}, vk::Filter::eLinear);

            barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
            barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
                                {}, {}, {}, {barrier});
            w = w > 1 ? w / 2 : 1;
            h = h > 1 ? h / 2 : 1;
        }
        vk::ImageMemoryBarrier toRead{};
        toRead.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        toRead.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        toRead.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
        toRead.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
        toRead.image = *_image;
        toRead.subresourceRange.aspectMask = aspect;
        toRead.subresourceRange.baseMipLevel = 0;
        toRead.subresourceRange.levelCount = _mipLevels;
        toRead.subresourceRange.baseArrayLayer = 0;
        toRead.subresourceRange.layerCount = 1;
        toRead.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        toRead.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                            {}, {}, {}, {toRead});
    });
    return ok;
}

void VKTexture2D::bind(unsigned int) {}

void* VKTexture2D::handle() {
    return reinterpret_cast<void*>(static_cast<VkImageView>(view()));
}

void VKTexture2D::release() {
    _sampler = vk::raii::Sampler{nullptr};
    _view = vk::raii::ImageView{nullptr};
    _memory = vk::raii::DeviceMemory{nullptr};
    _image = vk::raii::Image{nullptr};
    _adopted = false;
    _adoptedView = VK_NULL_HANDLE;
    _valid = false;
    _layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    _mipLevels = 1;
}

} // namespace rhi
