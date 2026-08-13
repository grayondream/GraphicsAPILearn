#include "VKRenderTarget.hpp"
#include "VKFormat.hpp"
#include "VKTexture3D.hpp"
#include "base/Log.hpp"
#include <vector>

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
            _colors.push_back(std::move(img));
            if (msaa) {
                Image res;
                if (!makeImage(res, att.format, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                               vk::SampleCountFlagBits::e1)) {
                    return false;
                }
                _resolved.push_back(std::move(res));
            }
        } else {
            if (_depthAttachment) continue;
            if (!makeImage(_depth, att.format, vk::ImageUsageFlagBits::eDepthStencilAttachment |
                            vk::ImageUsageFlagBits::eSampled, samples)) {
                return false;
            }
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
        ad.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
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
    if (_depthAttachment) {
        vk::AttachmentDescription ad{};
        ad.format = ToVkTextureFormat(_depth.format);
        ad.samples = msaa ? ToVkSamples(static_cast<int>(_samples)) : vk::SampleCountFlagBits::e1;
        ad.loadOp = vk::AttachmentLoadOp::eClear;
        ad.storeOp = vk::AttachmentStoreOp::eStore;
        ad.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        ad.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        ad.initialLayout = vk::ImageLayout::eUndefined;
        ad.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
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
    if (_depthAttachment) views.push_back(*_depth.view);

    vk::FramebufferCreateInfo fci{};
    fci.renderPass = *_renderPass;
    fci.attachmentCount = static_cast<uint32_t>(views.size());
    fci.pAttachments = views.data();
    fci.width = _extent.width;
    fci.height = _extent.height;
    fci.layers = 1;
    auto fr = _dev.createFramebuffer(fci);
    if (fr.result != vk::Result::eSuccess) {
        LOGE("VKRenderTarget: createFramebuffer failed");
        return false;
    }
    _framebuffer = std::move(fr.value);
    return true;
}

void VKRenderTarget::createWrappers() {
    _colorWrappers.clear();
    for (size_t i = 0; i < _colors.size(); i++) {
        vk::ImageView view = this->msaa() ? *_resolved[i].view : *_colors[i].view;
        auto wrap = std::make_shared<VKTexture2D>(_dev, _phys, _queue, _graphicsFamily);
        wrap->adopt(view, ToVkTextureFormat(_colors[i].format), _extent);
        _colorWrappers.push_back(std::move(wrap));
    }
    if (_depthAttachment) {
        _depthWrapper = std::make_shared<VKTexture2D>(_dev, _phys, _queue, _graphicsFamily);
        _depthWrapper->adopt(*_depth.view, ToVkTextureFormat(_depth.format), _extent);
    }
}

bool VKRenderTarget::attachCubeFace(ITexture3D* cube, int face, int mip) {
    auto vkCube = static_cast<VKTexture3D*>(cube);
    if (!vkCube || !vkCube->valid()) return false;
    if (_framebuffer != nullptr) _framebuffer = vk::raii::Framebuffer{nullptr};

    std::vector<vk::ImageView> views;
    if (!_colors.empty()) views.push_back(vkCube->faceView(face, mip));
    if (_depthAttachment) views.push_back(*_depth.view);

    vk::FramebufferCreateInfo fci{};
    fci.renderPass = *_renderPass;
    fci.attachmentCount = static_cast<uint32_t>(views.size());
    fci.pAttachments = views.data();
    fci.width = _extent.width;
    fci.height = _extent.height;
    fci.layers = 1;
    auto fr = _dev.createFramebuffer(fci);
    if (fr.result != vk::Result::eSuccess) return false;
    _framebuffer = std::move(fr.value);
    return true;
}

bool VKRenderTarget::attachDepthCube(ITexture3D* cube, int mip) {
    (void)mip;
    auto vkCube = static_cast<VKTexture3D*>(cube);
    if (!vkCube || !vkCube->valid()) return false;
    if (_framebuffer != nullptr) _framebuffer = vk::raii::Framebuffer{nullptr};

    std::vector<vk::ImageView> views;
    for (auto& c : _colors) views.push_back(*c.view);
    if (!_depthAttachment && _colorCount == 0) {
        views.push_back(vkCube->depthCubeView());
    } else if (_depthAttachment) {
        views.push_back(*_depth.view);
    }

    vk::FramebufferCreateInfo fci{};
    fci.renderPass = *_renderPass;
    fci.attachmentCount = static_cast<uint32_t>(views.size());
    fci.pAttachments = views.data();
    fci.width = _extent.width;
    fci.height = _extent.height;
    fci.layers = 1;
    auto fr = _dev.createFramebuffer(fci);
    if (fr.result != vk::Result::eSuccess) return false;
    _framebuffer = std::move(fr.value);
    return true;
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
    _colorCount = 0;
}

void VKRenderTarget::release() {
    clearImages();
    _valid = false;
    _samples = 1;
}

} // namespace rhi
