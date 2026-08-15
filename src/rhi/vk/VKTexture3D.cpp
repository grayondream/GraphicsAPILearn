#include "VKTexture3D.hpp"
#include "VKFormat.hpp"
#include "VKUpload.hpp"
#include "base/Log.hpp"
#include <cstring>

namespace rhi {

bool VKTexture3D::init(const TextureDataView3D& data) {
    release();
    _format = vk::Format::eR8G8B8A8Unorm;
    _extent = vk::Extent2D(static_cast<uint32_t>(data.width), static_cast<uint32_t>(data.height));
    _mipLevels = 1;
    _cube = false;
    _depth = false;

    vk::ImageCreateInfo ici{};
    ici.imageType = vk::ImageType::e3D;
    ici.format = _format;
    ici.extent = vk::Extent3D(static_cast<uint32_t>(data.width), static_cast<uint32_t>(data.height),
                              static_cast<uint32_t>(data.depth > 0 ? data.depth : 1));
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = vk::SampleCountFlagBits::e1;
    ici.tiling = vk::ImageTiling::eOptimal;
    ici.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
    ici.sharingMode = vk::SharingMode::eExclusive;
    ici.initialLayout = vk::ImageLayout::eUndefined;
    auto ir = _dev.createImage(ici);
    if (ir.result != vk::Result::eSuccess) return false;
    _image = std::move(ir.value);

    const vk::MemoryRequirements req = _image.getMemoryRequirements();
    const vk::PhysicalDeviceMemoryProperties props = _phys.getMemoryProperties();
    const uint32_t memType = findMemoryType(props, req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    if (memType == UINT32_MAX) return false;
    vk::MemoryAllocateInfo mai(req.size, memType);
    auto ar = _dev.allocateMemory(mai);
    if (ar.result != vk::Result::eSuccess) return false;
    _memory = std::move(ar.value);
    vk::BindImageMemoryInfo bimi(*_image, *_memory, 0);
    if (_dev.bindImageMemory2({bimi}) != vk::Result::eSuccess) return false;

    const size_t size = static_cast<size_t>(data.width) * static_cast<size_t>(data.height) *
                        static_cast<size_t>(data.depth > 0 ? data.depth : 1) * 4;
    if (data.data && !UploadStagingToImage(_dev, _phys, _queue, _graphicsFamily, *_image, data.data, size,
                                           ici.extent, 0, 1, vk::ImageAspectFlagBits::eColor,
                                           vk::ImageLayout::eShaderReadOnlyOptimal)) {
        return false;
    }

    vk::ImageViewCreateInfo vci{};
    vci.image = *_image;
    vci.viewType = vk::ImageViewType::e3D;
    vci.format = _format;
    vci.components.r = vk::ComponentSwizzle::eIdentity;
    vci.components.g = vk::ComponentSwizzle::eIdentity;
    vci.components.b = vk::ComponentSwizzle::eIdentity;
    vci.components.a = vk::ComponentSwizzle::eIdentity;
    vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    vci.subresourceRange.baseMipLevel = 0;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.baseArrayLayer = 0;
    vci.subresourceRange.layerCount = 1;
    auto vr = _dev.createImageView(vci);
    if (vr.result != vk::Result::eSuccess) return false;
    _cubeView = std::move(vr.value);

    vk::SamplerCreateInfo sci{};
    sci.magFilter = vk::Filter::eLinear;
    sci.minFilter = vk::Filter::eLinear;
    sci.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sci.addressModeU = vk::SamplerAddressMode::eRepeat;
    sci.addressModeV = vk::SamplerAddressMode::eRepeat;
    sci.addressModeW = vk::SamplerAddressMode::eRepeat;
    sci.minLod = 0.0f;
    sci.maxLod = 1.0f;
    auto sr = _dev.createSampler(sci);
    if (sr.result != vk::Result::eSuccess) return false;
    _sampler = std::move(sr.value);

    _valid = true;
    return true;
}

bool VKTexture3D::initCube(const TextureDesc& desc, const TextureDataView2D* faces) {
    release();
    _format = ToVkTextureFormat(desc.format);
    _extent = vk::Extent2D(static_cast<uint32_t>(faces[0].width), static_cast<uint32_t>(faces[0].height));
    _mipLevels = desc.generateMipmap ? ComputeMipLevels(faces[0].width, faces[0].height) : 1;
    _cube = true;
    _depth = false;

    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
    if (_mipLevels > 1) usage |= vk::ImageUsageFlagBits::eTransferSrc;
    if (!createCubeImage(faces[0].width, faces[0].height, _mipLevels, usage)) return false;

    for (int f = 0; f < 6; f++) {
        const size_t size = static_cast<size_t>(faces[f].width) * static_cast<size_t>(faces[f].height) *
                            ToVkTexelSize(desc.format);
        if (!UploadStagingToImage(_dev, _phys, _queue, _graphicsFamily, *_image, faces[f].data, size,
                                  vk::Extent3D(_extent.width, _extent.height, 1),
                                  static_cast<uint32_t>(f), 1, vk::ImageAspectFlagBits::eColor,
                                  vk::ImageLayout::eShaderReadOnlyOptimal)) {
            return false;
        }
    }
    if (_mipLevels > 1) {
        if (!genCubeMipmaps(faces[0].width, faces[0].height)) return false;
        _layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }
    if (!createSampler(desc)) return false;
    if (!createCubeViews(vk::ImageAspectFlagBits::eColor)) return false;
    _valid = true;
    return true;
}

bool VKTexture3D::createEmpty(const TextureDesc& desc, int width, int height) {
    release();
    _format = ToVkTextureFormat(desc.format);
    _extent = vk::Extent2D(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    _mipLevels = desc.generateMipmap ? ComputeMipLevels(width, height) : 1;
    _cube = true;
    _depth = ToVkAspect(desc.format) != vk::ImageAspectFlagBits::eColor;
    const vk::ImageAspectFlags aspect = ToVkAspect(desc.format);

    vk::ImageUsageFlags usage = _depth
        ? (vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled)
        : (vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
           vk::ImageUsageFlagBits::eTransferDst |
           (_mipLevels > 1 ? vk::ImageUsageFlagBits::eTransferSrc : vk::ImageUsageFlagBits{}));
    if (!createCubeImage(width, height, _mipLevels, usage)) return false;

    if (!_depth) {
        if (!TransitionImage(_dev, _queue, _graphicsFamily, *_image, vk::ImageLayout::eUndefined,
                             vk::ImageLayout::eShaderReadOnlyOptimal, aspect, 0, _mipLevels, 0, 6)) {
            return false;
        }
        _layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }
    if (!createSampler(desc)) return false;
    if (!createCubeViews(aspect)) return false;
    _valid = true;
    return true;
}

bool VKTexture3D::createCubeImage(int width, int height, uint32_t mipLevels,
                                  vk::ImageUsageFlags usage) {
    vk::ImageCreateInfo ici{};
    ici.flags = vk::ImageCreateFlagBits::eCubeCompatible;
    ici.imageType = vk::ImageType::e2D;
    ici.format = _format;
    ici.extent = vk::Extent3D(static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1);
    ici.mipLevels = mipLevels;
    ici.arrayLayers = 6;
    ici.samples = vk::SampleCountFlagBits::e1;
    ici.tiling = vk::ImageTiling::eOptimal;
    ici.usage = usage;
    ici.sharingMode = vk::SharingMode::eExclusive;
    ici.initialLayout = vk::ImageLayout::eUndefined;
    auto ir = _dev.createImage(ici);
    if (ir.result != vk::Result::eSuccess) {
        LOGE("VKTexture3D: createImage(cube) failed");
        return false;
    }
    _image = std::move(ir.value);

    const vk::MemoryRequirements req = _image.getMemoryRequirements();
    const vk::PhysicalDeviceMemoryProperties props = _phys.getMemoryProperties();
    const uint32_t memType = findMemoryType(props, req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    if (memType == UINT32_MAX) return false;
    vk::MemoryAllocateInfo mai(req.size, memType);
    auto ar = _dev.allocateMemory(mai);
    if (ar.result != vk::Result::eSuccess) return false;
    _memory = std::move(ar.value);
    vk::BindImageMemoryInfo bimi(*_image, *_memory, 0);
    return _dev.bindImageMemory2({bimi}) == vk::Result::eSuccess;
}

bool VKTexture3D::createCubeViews(vk::ImageAspectFlags aspect) {
    _faceViews.clear();
    _faceViews.reserve(_mipLevels * 6);
    for (uint32_t mip = 0; mip < _mipLevels; mip++) {
        for (uint32_t f = 0; f < 6; f++) {
            vk::ImageViewCreateInfo vci{};
            vci.image = *_image;
            vci.viewType = vk::ImageViewType::e2D;
            vci.format = _format;
            vci.components.r = vk::ComponentSwizzle::eIdentity;
            vci.components.g = vk::ComponentSwizzle::eIdentity;
            vci.components.b = vk::ComponentSwizzle::eIdentity;
            vci.components.a = vk::ComponentSwizzle::eIdentity;
            vci.subresourceRange.aspectMask = aspect;
            vci.subresourceRange.baseMipLevel = mip;
            vci.subresourceRange.levelCount = 1;
            vci.subresourceRange.baseArrayLayer = f;
            vci.subresourceRange.layerCount = 1;
            auto vr = _dev.createImageView(vci);
            if (vr.result != vk::Result::eSuccess) return false;
            _faceViews.push_back(std::move(vr.value));
        }
    }

    vk::ImageViewCreateInfo cvi{};
    cvi.image = *_image;
    cvi.viewType = vk::ImageViewType::eCube;
    cvi.format = _format;
    cvi.components.r = vk::ComponentSwizzle::eIdentity;
    cvi.components.g = vk::ComponentSwizzle::eIdentity;
    cvi.components.b = vk::ComponentSwizzle::eIdentity;
    cvi.components.a = vk::ComponentSwizzle::eIdentity;
    cvi.subresourceRange.aspectMask = aspect;
    cvi.subresourceRange.baseMipLevel = 0;
    cvi.subresourceRange.levelCount = _mipLevels;
    cvi.subresourceRange.baseArrayLayer = 0;
    cvi.subresourceRange.layerCount = 6;
    auto cvr = _dev.createImageView(cvi);
    if (cvr.result != vk::Result::eSuccess) return false;
    if (_depth) _depthCubeView = std::move(cvr.value);
    else _cubeView = std::move(cvr.value);
    return true;
}

bool VKTexture3D::createSampler(const TextureDesc& desc) {
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
    if (sr.result != vk::Result::eSuccess) return false;
    _sampler = std::move(sr.value);
    return true;
}

bool VKTexture3D::genCubeMipmaps(int width, int height) {
    const vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor;
    const bool ok = SubmitOneShot(_dev, _queue, _graphicsFamily, [&](vk::raii::CommandBuffer& cmd) {
        vk::ImageMemoryBarrier toSrc{};
        toSrc.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        toSrc.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        toSrc.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
        toSrc.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
        toSrc.image = *_image;
        toSrc.subresourceRange.aspectMask = aspect;
        toSrc.subresourceRange.baseMipLevel = 0;
        toSrc.subresourceRange.levelCount = _mipLevels;
        toSrc.subresourceRange.baseArrayLayer = 0;
        toSrc.subresourceRange.layerCount = 6;
        toSrc.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer,
                            {}, {}, {}, {toSrc});

        int32_t w = width;
        int32_t h = height;
        for (uint32_t mip = 1; mip < _mipLevels; mip++) {
            vk::ImageMemoryBarrier toDst{};
            toDst.oldLayout = vk::ImageLayout::eUndefined;
            toDst.newLayout = vk::ImageLayout::eTransferDstOptimal;
            toDst.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
            toDst.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
            toDst.image = *_image;
            toDst.subresourceRange.aspectMask = aspect;
            toDst.subresourceRange.baseMipLevel = mip;
            toDst.subresourceRange.levelCount = 1;
            toDst.subresourceRange.baseArrayLayer = 0;
            toDst.subresourceRange.layerCount = 6;
            toDst.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
                                {}, {}, {}, {toDst});

            vk::ImageBlit blit{};
            blit.srcSubresource.aspectMask = aspect;
            blit.srcSubresource.mipLevel = mip - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 6;
            blit.srcOffsets[0] = vk::Offset3D(0, 0, 0);
            blit.srcOffsets[1] = vk::Offset3D(w, h, 1);
            blit.dstSubresource.aspectMask = aspect;
            blit.dstSubresource.mipLevel = mip;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 6;
            blit.dstOffsets[0] = vk::Offset3D(0, 0, 0);
            blit.dstOffsets[1] = vk::Offset3D(w > 1 ? w / 2 : 1, h > 1 ? h / 2 : 1, 1);
            cmd.blitImage(*_image, vk::ImageLayout::eTransferSrcOptimal,
                          *_image, vk::ImageLayout::eTransferDstOptimal, {blit}, vk::Filter::eLinear);

            toDst.oldLayout = vk::ImageLayout::eTransferDstOptimal;
            toDst.newLayout = vk::ImageLayout::eTransferSrcOptimal;
            toDst.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            toDst.dstAccessMask = vk::AccessFlagBits::eTransferRead;
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
                                {}, {}, {}, {toDst});
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
        toRead.subresourceRange.layerCount = 6;
        toRead.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        toRead.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                            {}, {}, {}, {toRead});
    });
    return ok;
}

vk::ImageView VKTexture3D::faceView(int face, int mip) const {
    const size_t idx = static_cast<size_t>(mip) * 6 + static_cast<size_t>(face);
    if (idx >= _faceViews.size()) return vk::ImageView{nullptr};
    return *_faceViews[idx];
}

void VKTexture3D::bind(unsigned int) {}

void* VKTexture3D::handle() {
    return reinterpret_cast<void*>(static_cast<VkImageView>(*_cubeView));
}

void VKTexture3D::release() {
    _sampler = vk::raii::Sampler{nullptr};
    _faceViews.clear();
    _depthCubeView = vk::raii::ImageView{nullptr};
    _cubeView = vk::raii::ImageView{nullptr};
    _memory = vk::raii::DeviceMemory{nullptr};
    _image = vk::raii::Image{nullptr};
    _cube = false;
    _depth = false;
    _valid = false;
    _layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    _mipLevels = 1;
}

} // namespace rhi
