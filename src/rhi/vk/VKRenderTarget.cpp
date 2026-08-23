#include "VKRenderTarget.hpp"
#include "VKFormat.hpp"
#include "VKTexture3D.hpp"
#include "base/Log.hpp"
#include <limits>
#include <vector>
#include <cstdlib>

namespace rhi {

bool VKRenderTarget::create(int width, int height) {
    FramebufferDesc desc;
    desc.width = width;
    desc.height = height;
    FramebufferAttachment color;
    color.type = AttachmentType::Color;
    color.format = TextureFormat::RGBA8;
    desc.attachments.push_back(color);
    FramebufferAttachment depth;
    depth.type = AttachmentType::DepthStencil;
    depth.format = TextureFormat::Depth24Stencil8;
    desc.attachments.push_back(depth);
    return create(desc);
}

bool VKRenderTarget::create(const FramebufferDesc& desc) {
    release();
    _extent = vk::Extent2D(static_cast<uint32_t>(desc.width), static_cast<uint32_t>(desc.height));
    _samples = desc.samples > 0 ? static_cast<uint32_t>(desc.samples) : 1u;
    return buildFromDesc(desc);
}

bool VKRenderTarget::buildFromDesc(const FramebufferDesc& desc) {
    const vk::SampleCountFlagBits samples = ToVkSamples(static_cast<int>(_samples));
    const bool msaa = samples != vk::SampleCountFlagBits::e1;

    for (const auto& att : desc.attachments) {
        if (att.type == AttachmentType::Color) {
            Image img;
            if (!makeImage(img, att.format, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                           samples)) {
                return false;
            }
            img.minFilter = att.minFilter;
            img.magFilter = att.magFilter;
            img.wrapS = att.wrapS;
            img.wrapT = att.wrapT;
            img.wrapR = att.wrapS;
            _colors.push_back(std::move(img));
            if (msaa) {
                Image res;
                if (!makeImage(res, att.format, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                               vk::SampleCountFlagBits::e1)) {
                    return false;
                }
                res.minFilter = att.minFilter;
                res.magFilter = att.magFilter;
                res.wrapS = att.wrapS;
                res.wrapT = att.wrapT;
                res.wrapR = att.wrapS;
                _resolved.push_back(std::move(res));
            }
        } else {
            if (_depthAttachment) continue;
            if (!makeImage(_depth, att.format, vk::ImageUsageFlagBits::eDepthStencilAttachment |
                            vk::ImageUsageFlagBits::eSampled, samples)) {
                return false;
            }
            _depth.minFilter = att.minFilter;
            _depth.magFilter = att.magFilter;
            _depth.wrapS = att.wrapS;
            _depth.wrapT = att.wrapT;
            _depth.wrapR = att.wrapS;
            _depthAttachment = true;
        }
    }
    _colorCount = static_cast<uint32_t>(_colors.size());

    if (!createRenderPass()) return false;
    if (!createFramebuffer()) return false;
    createWrappers();
    _valid = true;
    return true;
}

bool VKRenderTarget::makeImage(Image& img, TextureFormat format, vk::ImageUsageFlags usage,
                               vk::SampleCountFlagBits samples, bool layered) {
    // llvmpipe/软渲染下 float 离屏 RT 无法可靠写入（HDR/Bloom/SSAO/Defer 等
    // 依赖 float RT 的 App 会整帧黑屏），回退为 RGBA8 保证画面正常。
    if (_floatRtFallback &&
        (format == TextureFormat::RGBA16F || format == TextureFormat::RGB16F ||
         format == TextureFormat::RGBA32F || format == TextureFormat::RG16F ||
         format == TextureFormat::R32F)) {
        LOGI("VKRenderTarget: float RT {} -> RGBA8 (llvmpipe fallback)", static_cast<int>(format));
        format = TextureFormat::RGBA8;
    }
    // R8G8B8_UNORM 不是 Vulkan 强制支持的颜色附件格式，llvmpipe 上
    // 渲染/采样异常（内容全白/错乱），统一回退为 RGBA8。
    if (format == TextureFormat::RGB8) {
        LOGI("VKRenderTarget: RGB8 -> RGBA8 (R8G8B8_UNORM 非强制颜色附件格式)");
        format = TextureFormat::RGBA8;
    }
    img.format = format;
    const vk::Format vkFormat = ToVkTextureFormat(format);
    vk::ImageCreateInfo ici{};
    ici.imageType = vk::ImageType::e2D;
    ici.format = vkFormat;
    ici.extent = vk::Extent3D(_extent.width, _extent.height, 1);
    ici.mipLevels = 1;
    ici.arrayLayers = layered ? 6 : 1;
    ici.samples = samples;
    ici.tiling = vk::ImageTiling::eOptimal;
    ici.usage = usage;
    ici.sharingMode = vk::SharingMode::eExclusive;
    ici.initialLayout = vk::ImageLayout::eUndefined;
    auto ir = _dev.createImage(ici);
    if (ir.result != vk::Result::eSuccess) {
        LOGE("VKRenderTarget: createImage failed");
        return false;
    }
    img.image = std::move(ir.value);

    const vk::MemoryRequirements req = img.image.getMemoryRequirements();
    const vk::PhysicalDeviceMemoryProperties props = _phys.getMemoryProperties();
    const uint32_t memType = findMemoryType(props, req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    if (memType == UINT32_MAX) return false;
    vk::MemoryAllocateInfo mai(req.size, memType);
    auto ar = _dev.allocateMemory(mai);
    if (ar.result != vk::Result::eSuccess) return false;
    img.memory = std::move(ar.value);
    vk::BindImageMemoryInfo bimi(*img.image, *img.memory, 0);
    if (_dev.bindImageMemory2({bimi}) != vk::Result::eSuccess) return false;

    vk::ImageViewCreateInfo vci{};
    vci.image = *img.image;
    vci.viewType = layered ? vk::ImageViewType::eCube : vk::ImageViewType::e2D;
    vci.format = vkFormat;
    vci.components.r = vk::ComponentSwizzle::eIdentity;
    vci.components.g = vk::ComponentSwizzle::eIdentity;
    vci.components.b = vk::ComponentSwizzle::eIdentity;
    vci.components.a = vk::ComponentSwizzle::eIdentity;
    vci.subresourceRange.aspectMask = ToVkAspect(format);
    vci.subresourceRange.baseMipLevel = 0;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.baseArrayLayer = 0;
    vci.subresourceRange.layerCount = layered ? 6 : 1;
    auto vr = _dev.createImageView(vci);
    if (vr.result != vk::Result::eSuccess) {
        LOGE("VKRenderTarget: createImageView failed");
        return false;
    }
    img.view = std::move(vr.value);
    return true;
}

bool VKRenderTarget::createRenderPass() {
    const bool msaa = this->msaa();
    std::vector<vk::AttachmentDescription> attachments;
    std::vector<vk::AttachmentReference> colorRefs;
    std::vector<vk::AttachmentReference> resolveRefs;

for (const auto& c : _colors) {
        vk::AttachmentDescription ad{};
        ad.format = ToVkTextureFormat(c.format);
        ad.samples = msaa ? ToVkSamples(static_cast<int>(_samples)) : vk::SampleCountFlagBits::e1;
        ad.loadOp = vk::AttachmentLoadOp::eClear;
        ad.storeOp = vk::AttachmentStoreOp::eStore;
        ad.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        ad.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        ad.initialLayout = vk::ImageLayout::eUndefined;
        // Non-MSAA color attachments must be left in a samplerable layout so the
        // adopted colorTexture2D(i) can be sampled (e.g. post-processing).
        ad.finalLayout = msaa ? vk::ImageLayout::eColorAttachmentOptimal
                              : vk::ImageLayout::eShaderReadOnlyOptimal;
        uint32_t idx = static_cast<uint32_t>(attachments.size());
        attachments.push_back(ad);
        colorRefs.push_back(vk::AttachmentReference(idx, vk::ImageLayout::eColorAttachmentOptimal));
    }
    // Color-only cubemap capture (PBR IBL): the attached cube face is the sole
    // color attachment even though the RT owns no color image.
    if (_cubeColor && _colors.empty()) {
        vk::AttachmentDescription ad{};
        ad.format = _cubeColorFormat;
        ad.samples = vk::SampleCountFlagBits::e1;
        ad.loadOp = vk::AttachmentLoadOp::eClear;
        ad.storeOp = vk::AttachmentStoreOp::eStore;
        ad.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        ad.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        ad.initialLayout = vk::ImageLayout::eUndefined;
        ad.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        uint32_t idx = static_cast<uint32_t>(attachments.size());
        attachments.push_back(ad);
        colorRefs.push_back(vk::AttachmentReference(idx, vk::ImageLayout::eColorAttachmentOptimal));
    }

    if (msaa) {
        for (const auto& r : _resolved) {
            vk::AttachmentDescription ad{};
            ad.format = ToVkTextureFormat(r.format);
            ad.samples = vk::SampleCountFlagBits::e1;
            ad.loadOp = vk::AttachmentLoadOp::eDontCare;
            ad.storeOp = vk::AttachmentStoreOp::eStore;
            ad.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            ad.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            ad.initialLayout = vk::ImageLayout::eUndefined;
            ad.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            uint32_t idx = static_cast<uint32_t>(attachments.size());
            attachments.push_back(ad);
            resolveRefs.push_back(vk::AttachmentReference(idx, vk::ImageLayout::eColorAttachmentOptimal));
        }
    }

    vk::AttachmentReference depthRef{};
    bool hasDepthRef = false;
    if (_depthAttachment || _cubeDepth) {
        vk::AttachmentDescription ad{};
        ad.format = _cubeDepth ? _cubeDepthFormat : ToVkTextureFormat(_depth.format);
        // An attached depth cubemap is single-sampled; only the RT's own MSAA
        // depth can be multisampled.
        ad.samples = _cubeDepth ? vk::SampleCountFlagBits::e1
                                : (msaa ? ToVkSamples(static_cast<int>(_samples)) : vk::SampleCountFlagBits::e1);
        ad.loadOp = vk::AttachmentLoadOp::eClear;
        ad.storeOp = vk::AttachmentStoreOp::eStore;
        ad.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        ad.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        ad.initialLayout = vk::ImageLayout::eUndefined;
        // Leave depth in a samplerable layout so depthTexture2D() can be sampled.
        ad.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        depthRef = vk::AttachmentReference(static_cast<uint32_t>(attachments.size()),
                                           vk::ImageLayout::eDepthStencilAttachmentOptimal);
        hasDepthRef = true;
        attachments.push_back(ad);
    }

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
    subpass.pColorAttachments = colorRefs.data();
    if (msaa && !resolveRefs.empty()) subpass.pResolveAttachments = resolveRefs.data();
    if (hasDepthRef) subpass.pDepthStencilAttachment = &depthRef;

    vk::RenderPassCreateInfo rpci{};
    rpci.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpci.pAttachments = attachments.data();
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    auto rpr = _dev.createRenderPass(rpci);
    if (rpr.result != vk::Result::eSuccess) {
        LOGE("VKRenderTarget: createRenderPass failed");
        return false;
    }
    _renderPass = std::move(rpr.value);
    return true;
}

bool VKRenderTarget::createFramebuffer() {
    std::vector<vk::ImageView> views;
    for (auto& c : _colors) views.push_back(*c.view);
    if (this->msaa()) for (auto& r : _resolved) views.push_back(*r.view);
    if (_depthAttachment || _cubeDepth) views.push_back(_cubeDepth ? _cubeDepthView : *_depth.view);

    vk::FramebufferCreateInfo fci{};
    fci.renderPass = *_renderPass;
    fci.attachmentCount = static_cast<uint32_t>(views.size());
    fci.pAttachments = views.data();
    fci.width = _extent.width;
    fci.height = _extent.height;
    // A depth-only cubemap attachment (6 layers) is written to all faces via
    // the geometry shader (gl_Layer). Mixed color+cube combos keep 1 layer to
    // stay valid against single-layer color attachments.
    fci.layers = (_colorCount == 0 && _cubeDepth) ? 6u : 1u;
    auto fr = _dev.createFramebuffer(fci);
    if (fr.result != vk::Result::eSuccess) {
        LOGE("VKRenderTarget: createFramebuffer failed");
        return false;
    }
    _framebuffer = std::move(fr.value);
    _fbExtent = _extent;
    return true;
}

void VKRenderTarget::createWrappers() {
    _colorWrappers.clear();
    for (size_t i = 0; i < _colors.size(); i++) {
        vk::ImageView view = this->msaa() ? *_resolved[i].view : *_colors[i].view;
        auto wrap = std::make_shared<VKTexture2D>(_dev, _phys, _queue, _graphicsFamily);
        const Image& a = _colors[i];
        TextureDesc td;
        td.minFilter = a.minFilter;
        td.magFilter = a.magFilter;
        td.wrapS = a.wrapS;
        td.wrapT = a.wrapT;
        wrap->adopt(view, ToVkTextureFormat(a.format), _extent, td);
        _colorWrappers.push_back(std::move(wrap));
    }
    if (_depthAttachment) {
        _depthWrapper = std::make_shared<VKTexture2D>(_dev, _phys, _queue, _graphicsFamily);
        TextureDesc td;
        td.minFilter = _depth.minFilter;
        td.magFilter = _depth.magFilter;
        td.wrapS = _depth.wrapS;
        td.wrapT = _depth.wrapT;
        _depthWrapper->adopt(*_depth.view, ToVkTextureFormat(_depth.format), _extent, td);
    }
}

bool VKRenderTarget::attachCubeFace(ITexture3D* cube, int face, int mip) {
    auto vkCube = static_cast<VKTexture3D*>(cube);
    if (!vkCube || !vkCube->valid()) return false;
    if (_framebuffer != nullptr) _framebuffer = vk::raii::Framebuffer{nullptr};

    std::vector<vk::ImageView> views;
    if (_cubeDepth) {
        // Depth-only cubemap: attach the requested face's single-layer depth view
        // (render pass from attachDepthCube already carries the depth attachment).
        views.push_back(vkCube->faceView(face, mip));
    } else if (_colors.empty()) {
        // Color-only cubemap (PBR IBL): the cube face becomes the sole color
        // attachment. First attach wires the cube format into the render pass,
        // subsequent faces/mips reuse it (format is fixed per cube).
        if (!_cubeColor) {
            _cubeColor = true;
            _cubeColorFormat = vkCube->format();
            LOGI("attachCubeFace: color-only cube fmt={} face={} mip={}", static_cast<int>(_cubeColorFormat), face, mip);
            _renderPass = vk::raii::RenderPass{nullptr};
            if (!createRenderPass()) return false;
        }
        views.push_back(vkCube->faceView(face, mip));
        if (_depthAttachment) views.push_back(*_depth.view);
    }
    if (views.empty()) return false;

    // Framebuffer 必须与所附 cube 面（含 mip）实际尺寸一致：Vulkan 不像 GL 那样
    // 自动把光栅裁剪到附件大小，FB 尺寸大于附件图像时 clear/draw 会越界写。
    const vk::Extent2D faceExt = vkCube->mipExtent(static_cast<uint32_t>(mip));
    vk::FramebufferCreateInfo fci{};
    fci.renderPass = *_renderPass;
    fci.attachmentCount = static_cast<uint32_t>(views.size());
    fci.pAttachments = views.data();
    fci.width = faceExt.width;
    fci.height = faceExt.height;
    fci.layers = 1;
    _fbExtent = faceExt;
    auto fr = _dev.createFramebuffer(fci);
    if (fr.result != vk::Result::eSuccess) return false;
    _framebuffer = std::move(fr.value);
    return true;
}

bool VKRenderTarget::attachDepthCube(ITexture3D* cube, int mip) {
    (void)mip;
    auto vkCube = static_cast<VKTexture3D*>(cube);
    if (!vkCube || !vkCube->valid()) return false;
    vk::ImageView view = vkCube->depthCubeView();
    if (!view) return false;

    // Wire the cube's layered depth view in as this render pass's depth
    // attachment, then rebuild render pass + framebuffer so their attachment
    // sets match (cube depth is added regardless of whether the RT owns color
    // and/or a depth attachment).
    _cubeDepthView = view;
    _cubeDepthFormat = vkCube->format();
    _cubeDepth = true;

    _framebuffer = vk::raii::Framebuffer{nullptr};
    _renderPass = vk::raii::RenderPass{nullptr};
    if (!createRenderPass()) return false;
    return createFramebuffer();
}

bool VKRenderTarget::bind() { return true; }
bool VKRenderTarget::unbind() { return true; }

void* VKRenderTarget::colorTexture() {
    if (_colors.empty()) return nullptr;
    return reinterpret_cast<void*>(static_cast<VkImageView>(*_colors[0].view));
}

ITexture2D* VKRenderTarget::colorTexture2D(int attachment) {
    if (attachment < 0 || static_cast<size_t>(attachment) >= _colorWrappers.size()) return nullptr;
    return _colorWrappers[static_cast<size_t>(attachment)].get();
}

ITexture2D* VKRenderTarget::depthTexture2D() {
    return _depthWrapper ? _depthWrapper.get() : nullptr;
}

bool VKRenderTarget::resolveTo(IRenderTarget& dst) {
    (void)dst;
    return true;
}

void* VKRenderTarget::handle() {
    return reinterpret_cast<void*>(static_cast<VkFramebuffer>(*_framebuffer));
}

vk::Image VKRenderTarget::colorImage(uint32_t i) const {
    if (i >= _colors.size()) return vk::Image{nullptr};
    if (_samples > 1 && i < _resolved.size()) return *_resolved[i].image;
    return *_colors[i].image;
}

vk::Format VKRenderTarget::colorFormat(uint32_t i) const {
    if (i >= _colors.size()) return vk::Format::eR8G8B8A8Unorm;
    return ToVkTextureFormat(_colors[i].format);
}

vk::Image VKRenderTarget::depthImage() const {
    return _depthAttachment ? *_depth.image : vk::Image{nullptr};
}

void VKRenderTarget::clearImages() {
    _depthWrapper.reset();
    _colorWrappers.clear();
    _framebuffer = vk::raii::Framebuffer{nullptr};
    _renderPass = vk::raii::RenderPass{nullptr};
    _resolved.clear();
    _colors.clear();
    _depth = Image{};
    _depthAttachment = false;
    _cubeDepth = false;
    _cubeDepthView = vk::ImageView{};
    _cubeDepthFormat = vk::Format{};
    _cubeColor = false;
    _cubeColorFormat = vk::Format{};
    _colorCount = 0;
}

void VKRenderTarget::release() {
    clearImages();
    _valid = false;
    _samples = 1;
}

void VKRenderTarget::debugDumpPPM(const char* path, uint32_t colorAtt) const {
    if (colorAtt >= _colors.size()) return;
    const vk::Image img = colorImage(colorAtt);
    const vk::Format fmt = colorFormat(colorAtt);
    if (!img) return;
    _dev.waitIdle();

    const uint64_t stagesize = static_cast<uint64_t>(_extent.width) * _extent.height * 4;
    vk::BufferCreateInfo bci{};
    bci.size = stagesize;
    bci.usage = vk::BufferUsageFlagBits::eTransferDst;
    bci.sharingMode = vk::SharingMode::eExclusive;
    auto br = _dev.createBuffer(bci);
    if (br.result != vk::Result::eSuccess) { return; }
    vk::raii::Buffer stage = std::move(br.value);
    const vk::MemoryRequirements sreq = stage.getMemoryRequirements();
    const vk::PhysicalDeviceMemoryProperties props = _phys.getMemoryProperties();
    uint32_t smemIdx = UINT32_MAX;
    for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
        if ((sreq.memoryTypeBits & (1u << i)) &&
            (props.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) &&
            (props.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent)) { smemIdx = i; break; }
    }
    if (smemIdx == UINT32_MAX) { LOGE("debugDump: no host mem"); return; }
    vk::MemoryAllocateInfo smai(sreq.size, smemIdx);
    auto sar = _dev.allocateMemory(smai);
    if (sar.result != vk::Result::eSuccess) return;
    vk::raii::DeviceMemory stageMem = std::move(sar.value);
    vk::BindBufferMemoryInfo sbbmi(*stage, *stageMem, 0);
    _dev.bindBufferMemory2({sbbmi});

    vk::CommandPoolCreateInfo cpci{};
    cpci.flags = vk::CommandPoolCreateFlagBits::eTransient;
    cpci.queueFamilyIndex = _graphicsFamily;
    auto cpr = _dev.createCommandPool(cpci);
    if (cpr.result != vk::Result::eSuccess) return;
    vk::raii::CommandPool pool = std::move(cpr.value);
    vk::CommandBufferAllocateInfo cba(*pool, vk::CommandBufferLevel::ePrimary, 1);
    auto cbr = _dev.allocateCommandBuffers(cba);
    if (cbr.result != vk::Result::eSuccess) return;
    vk::raii::CommandBuffer cb = std::move(cbr.value[0]);

    const vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor;
    vk::CommandBufferBeginInfo cbbi(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cb.begin(cbbi);
    const auto setBarrier = [&](const vk::Image& image, vk::ImageLayout oldL, vk::ImageLayout newL,
                                vk::AccessFlags srcA, vk::AccessFlags dstA,
                                vk::PipelineStageFlags srcS, vk::PipelineStageFlags dstS,
                                vk::ImageAspectFlags asp) {
        vk::ImageMemoryBarrier barrier{};
        barrier.srcAccessMask = srcA;
        barrier.dstAccessMask = dstA;
        barrier.oldLayout = oldL;
        barrier.newLayout = newL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = asp;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        cb.pipelineBarrier(srcS, dstS, {}, {}, {}, {barrier});
    };
    setBarrier(img, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferSrcOptimal,
               vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead,
               vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer, aspect);
    vk::BufferImageCopy region{};
    region.imageSubresource.aspectMask = aspect;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D(0, 0, 0);
    region.imageExtent = vk::Extent3D(_extent.width, _extent.height, 1);
    cb.copyImageToBuffer(img, vk::ImageLayout::eTransferSrcOptimal, *stage, {region});
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

    auto mp = stageMem.mapMemory(0, stagesize);
    if (mp.result != vk::Result::eSuccess) return;
    const unsigned char* px = static_cast<const unsigned char*>(mp.value);
    FILE* fp = std::fopen(path, "wb");
    if (!fp) { stageMem.unmapMemory(); return; }
    std::fprintf(fp, "P6\n%u %u\n255\n", _extent.width, _extent.height);
    const bool isF16 = (fmt == vk::Format::eR16G16B16A16Sfloat);
    for (uint32_t y = 0; y < _extent.height; y++) {
        for (uint32_t x = 0; x < _extent.width; x++) {
            float r = 0, g = 0, b = 0, a = 1.0f;
            if (isF16) {
                const uint16_t* fp16 = reinterpret_cast<const uint16_t*>(px + static_cast<size_t>(y) * _extent.width * 8 + static_cast<size_t>(x) * 8);
                const auto conv = [](uint16_t half) {
                    uint32_t sign = (half >> 15) & 1, exp = (half >> 10) & 0x1f, man = half & 0x3ff;
                    if (exp == 0) return man == 0 ? 0.0f : (man / 1024.0f) * (1 / 16384.0f);
                    if (exp == 31) return man == 0 ? (sign ? -1e30f : 1e30f) : std::numeric_limits<float>::quiet_NaN();
                    int e = static_cast<int>(exp) - 15;
                    float m = 1.0f + man / 1024.0f;
                    float v = m * std::pow(2.0f, static_cast<float>(e));
                    return sign ? -v : v;
                };
                r = conv(fp16[0]); g = conv(fp16[1]); b = conv(fp16[2]); a = conv(fp16[3]);
            } else {
                const unsigned char* p = px + static_cast<size_t>(y) * _extent.width * 4 + static_cast<size_t>(x) * 4;
                r = p[2] / 255.0f; g = p[1] / 255.0f; b = p[0] / 255.0f; a = p[3] / 255.0f;
            }
            float rr = std::fmin(1.0f, std::fmax(0.0f, r * a));
            float gg = std::fmin(1.0f, std::fmax(0.0f, g * a));
            float bb = std::fmin(1.0f, std::fmax(0.0f, b * a));
            std::fputc(static_cast<int>(rr * 255.0f + 0.5f), fp);
            std::fputc(static_cast<int>(gg * 255.0f + 0.5f), fp);
            std::fputc(static_cast<int>(bb * 255.0f + 0.5f), fp);
        }
    }
    std::fclose(fp);
    stageMem.unmapMemory();
    LOGI("debugDumpPPM: saved {} ({}x{})", path, _extent.width, _extent.height);
}

} // namespace rhi
