#pragma once
#include "VKHeader.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "VKTexture2D.hpp"

namespace rhi {

class VKTexture3D;

class VKRenderTarget : public IRenderTarget {
public:
    VKRenderTarget(vk::raii::Device& device, vk::raii::PhysicalDevice& phys,
                   vk::raii::Queue& queue, uint32_t graphicsFamily,
                   bool floatRtFallback = false)
        : _dev(device), _phys(phys), _queue(queue), _graphicsFamily(graphicsFamily),
          _floatRtFallback(floatRtFallback) {}

    bool create(int width, int height) override;
    bool create(const FramebufferDesc& desc) override;
    bool attachCubeFace(ITexture3D* cube, int face, int mip = 0) override;
    bool attachDepthCube(ITexture3D* cube, int mip = 0) override;
    bool bind() override;
    bool unbind() override;
    void* colorTexture() override;
    ITexture2D* colorTexture2D(int attachment = 0) override;
    ITexture2D* depthTexture2D() override;
    bool resolveTo(IRenderTarget& dst) override;
    void* handle() override;
    void release() override;

    vk::RenderPass renderPass() const { return _renderPass != nullptr ? *_renderPass : vk::RenderPass{nullptr}; }
    vk::Framebuffer framebuffer() const { return _framebuffer != nullptr ? *_framebuffer : vk::Framebuffer{nullptr}; }
    vk::Extent2D extent2d() const { return _extent; }
    vk::Extent2D framebufferExtent() const { return (_fbExtent.width != 0) ? _fbExtent : _extent; }
    // True attachment count of the render pass, in attachment order:
    // [color msaa] x N, [resolve] x N (when MSAA), [depth] (optional).
    uint32_t attachmentCount() const {
        uint32_t n = _colorCount;
        if (this->msaa()) n += _colorCount;
        if (_cubeColor && _colorCount == 0) n += 1;
        if (_depthAttachment || _cubeDepth) n += 1;
        return n;
    }
    // A color-only cubemap face attached via attachCubeFace acts as the sole
    // color attachment of the pass (PBR IBL capture), even though the RT owns
    // no color image. Expose it so pipelines blend a matching color attachment.
    uint32_t colorCount() const {
        if (_colorCount > 0) return _colorCount;
        return _cubeColor ? 1u : 0u;
    }
    bool msaa() const { return _samples > 1; }
    uint32_t samples() const { return _samples; }
    bool hasDepthAttachment() const { return _depthAttachment || _cubeDepth; }
    bool valid() const { return _valid; }
    vk::Image colorImage(uint32_t i) const;
    vk::Format colorFormat(uint32_t i) const;
    vk::Image depthImage() const;
    void debugDumpPPM(const char* path, uint32_t colorAtt = 0) const;

private:
    struct Image {
        vk::raii::Image image{nullptr};
        vk::raii::DeviceMemory memory{nullptr};
        vk::raii::ImageView view{nullptr};
        TextureFormat format{TextureFormat::RGBA8};
        TextureFilter minFilter{TextureFilter::Linear};
        TextureFilter magFilter{TextureFilter::Linear};
        TextureWrap wrapS{TextureWrap::ClampToEdge};
        TextureWrap wrapT{TextureWrap::ClampToEdge};
        TextureWrap wrapR{TextureWrap::ClampToEdge};
    };

    bool buildFromDesc(const FramebufferDesc& desc);
    bool createRenderPass();
    bool createFramebuffer();
    void createWrappers();
    bool makeImage(Image& img, TextureFormat format, vk::ImageUsageFlags usage,
                   vk::SampleCountFlagBits samples, bool layered = false);
    void clearImages();

    vk::raii::Device& _dev;
    vk::raii::PhysicalDevice& _phys;
    vk::raii::Queue& _queue;
    uint32_t _graphicsFamily{0};

    vk::Extent2D _extent{};
    vk::Extent2D _fbExtent{}; // 当前 framebuffer 实际尺寸（cube face attach 时可能小于 _extent）
    uint32_t _samples{1};
    uint32_t _colorCount{0};
    bool _floatRtFallback{false};
    std::vector<Image> _colors{};
    std::vector<Image> _resolved{};
    Image _depth;
    bool _depthAttachment{false};
    // Attached depth cubemap (point-light shadow): view + format, active after attachDepthCube.
    bool _cubeDepth{false};
    vk::ImageView _cubeDepthView{};
    vk::Format _cubeDepthFormat{};
    // Attached color cubemap face (PBR IBL): active after first attachCubeFace on
    // an RT that owns no color image. The cube face is the pass's color attachment.
    bool _cubeColor{false};
    vk::Format _cubeColorFormat{};

    vk::raii::RenderPass _renderPass{nullptr};
    vk::raii::Framebuffer _framebuffer{nullptr};

    std::vector<std::shared_ptr<VKTexture2D>> _colorWrappers{};
    std::shared_ptr<VKTexture2D> _depthWrapper{};
    bool _valid{false};
};

} // namespace rhi
