#include "VKTexture2D.hpp"
#include "VKFormat.hpp"
#include "VKUpload.hpp"
#include "base/Log.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

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
    // 16F 格式：GL 端 glTexImage2D 以 GL_FLOAT 源数据 + GL_RGB16F/RGBA16F 内部格式
    // 由驱动完成 float32→half 转换；VK 无此隐式转换，需在此将 float32 源数据
    // （HDR 贴图 stbi_loadf 输出 RGBA32F 布局）转成 half（RGBA16F）再上传。
    // 否则按 RGBA16F 的 8 字节/texel 上传 float 数据会错位 → 采样出纯绿/乱色。
    bool converted = false;
    std::vector<uint16_t> halfData;
    const vk::Format fmt = ToVkTextureFormat(desc.format);
    if ((desc.format == TextureFormat::RGBA16F || desc.format == TextureFormat::RGB16F) &&
        fmt == vk::Format::eR16G16B16A16Sfloat) {
        const int srcCh = data.channels == 3 ? 3 : 4;
        const size_t n = static_cast<size_t>(data.width) * static_cast<size_t>(data.height);
        halfData.resize(n * 4);
        const float* src = static_cast<const float*>(data.data);
        const auto toHalf = [](float f) {
            const uint32_t x = *reinterpret_cast<uint32_t*>(&f);
            const uint32_t sign = (x >> 16) & 0x8000;
            int32_t exp = static_cast<int32_t>((x >> 23) & 0xff) - 127 + 15;
            uint32_t man = x & 0x7fffff;
            if (exp >= 31) return static_cast<uint16_t>(sign | 0x7c00);
            if (exp <= 0) {
                if (exp < -10) return static_cast<uint16_t>(sign);
                man |= 0x800000;
                const uint32_t shift = static_cast<uint32_t>(14 - exp);
                return static_cast<uint16_t>(sign | (man >> shift));
            }
            return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exp) << 10) | (man >> 13));
        };
        for (size_t i = 0; i < n; i++) {
            halfData[i * 4 + 0] = toHalf(src[i * srcCh + 0]);
            halfData[i * 4 + 1] = toHalf(src[i * srcCh + 1]);
            halfData[i * 4 + 2] = toHalf(src[i * srcCh + 2]);
            halfData[i * 4 + 3] = srcCh == 4 ? toHalf(src[i * srcCh + 3]) : 0x3c00;
        }
        converted = true;
    }
    const void* uploadData = converted ? static_cast<const void*>(halfData.data()) : data.data;
    if (!UploadStagingToImage(_dev, _phys, _queue, _graphicsFamily, *_image, uploadData, size,
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

void VKTexture2D::debugDumpToPPM(const std::string& path) {
    (void)path;
    if (!_valid) return;
    const uint32_t w = _extent.width, h = _extent.height;
    const uint32_t n = w * h;
    const size_t bytes = n * 8;  // RGBA16F

    vk::BufferCreateInfo bci{};
    bci.size = bytes;
    bci.usage = vk::BufferUsageFlagBits::eTransferDst;
    bci.sharingMode = vk::SharingMode::eExclusive;
    auto br = _dev.createBuffer(bci);
    if (br.result != vk::Result::eSuccess) return;
    vk::raii::Buffer stage = std::move(br.value);
    const vk::MemoryRequirements sreq = stage.getMemoryRequirements();
    const vk::PhysicalDeviceMemoryProperties props = _phys.getMemoryProperties();
    uint32_t smemIdx = UINT32_MAX;
    for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
        if ((sreq.memoryTypeBits & (1u << i)) &&
            (props.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) &&
            (props.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent)) { smemIdx = i; break; }
    }
    if (smemIdx == UINT32_MAX) return;
    vk::MemoryAllocateInfo smai(sreq.size, smemIdx);
    auto sar = _dev.allocateMemory(smai);
    if (sar.result != vk::Result::eSuccess) return;
    vk::raii::DeviceMemory stageMem = std::move(sar.value);
    vk::BindBufferMemoryInfo sbbmi(*stage, *stageMem, 0);
    _dev.bindBufferMemory2({sbbmi});

    vk::CommandPoolCreateInfo cpci{};
    cpci.queueFamilyIndex = _graphicsFamily;
    auto cpr = _dev.createCommandPool(cpci);
    if (cpr.result != vk::Result::eSuccess) return;
    vk::raii::CommandPool pool = std::move(cpr.value);
    vk::CommandBufferAllocateInfo cba(*pool, vk::CommandBufferLevel::ePrimary, 1);
    auto cbr = _dev.allocateCommandBuffers(cba);
    if (cbr.result != vk::Result::eSuccess) return;
    vk::raii::CommandBuffer cb = std::move(cbr.value[0]);

    cb.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    vk::ImageMemoryBarrier b1{};
    b1.srcAccessMask = vk::AccessFlagBits::eShaderRead;
    b1.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    b1.oldLayout = _layout;
    b1.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    b1.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    b1.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    b1.image = *_image;
    b1.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer,
                       {}, {}, {}, {b1});
    vk::BufferImageCopy region{};
    region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    region.imageExtent = vk::Extent3D(w, h, 1);
    cb.copyImageToBuffer(*_image, vk::ImageLayout::eTransferSrcOptimal, *stage, {region});
    vk::ImageMemoryBarrier b2 = b1;
    b2.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    b2.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    b2.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    b2.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                       {}, {}, {}, {b2});
    cb.end();

    vk::FenceCreateInfo fci{};
    auto fr = _dev.createFence(fci);
    if (fr.result != vk::Result::eSuccess) return;
    vk::raii::Fence fence = std::move(fr.value);
    vk::CommandBuffer raw = *cb;
    vk::SubmitInfo si{};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &raw;
    _queue.submit({si}, *fence);
    (void)_dev.waitForFences({*fence}, vk::True, UINT64_MAX);

    auto mp = stageMem.mapMemory(0, bytes);
    if (mp.result != vk::Result::eSuccess) return;
    const uint16_t* hf = static_cast<const uint16_t*>(mp.value);
    FILE* f = fopen(path.c_str(), "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", (int)w, (int)h);
        const auto f16 = [](uint16_t v) {
            uint32_t s = (v >> 15) & 1, e = (v >> 10) & 0x1f, m = v & 0x3ff;
            float out = 0.0f;
            if (e == 0) out = (m == 0) ? 0.0f : ldexp((float)m / 1024.0f, -14);
            else if (e == 31) out = (m == 0) ? 1e30f : 0.0f;
            else out = ldexp(1.0f + (float)m / 1024.0f, e - 15);
            return s ? -out : out;
        };
        for (uint32_t i = 0; i < n; i++) {
            float r = f16(hf[i * 4 + 0]), g = f16(hf[i * 4 + 1]), b = f16(hf[i * 4 + 2]);
            auto enc = [](float x) { return (uint8_t)(std::min(1.0f, std::max(0.0f, x)) * 255.0f); };
            fputc(enc(r), f); fputc(enc(g), f); fputc(enc(b), f);
        }
        fclose(f);
    }
    stageMem.unmapMemory();
}
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
