#pragma once
#include "VKHeader.hpp"
#include "rhi/core/ITexture2D.hpp"

namespace rhi {

class VKTexture2D : public ITexture2D {
public:
    VKTexture2D(vk::raii::Device& device, vk::raii::PhysicalDevice& phys,
                vk::raii::Queue& queue, uint32_t graphicsFamily)
        : _dev(device), _phys(phys), _queue(queue), _graphicsFamily(graphicsFamily) {}

    bool init(const TextureDataView2D& data) override;
    bool init(const TextureDesc& desc, const TextureDataView2D& data) override;
    bool createEmpty(const TextureDesc& desc, int width, int height) override;
    void bind(unsigned int unit) override;
    void* handle() override;
    bool valid() const override { return _valid; }
    void release() override;

    bool adopt(vk::ImageView view, vk::Format format, vk::Extent2D ext, const TextureDesc& desc);
    void debugDumpToPPM(const std::string& path);

    vk::Sampler sampler() const { return _sampler != nullptr ? *_sampler : vk::Sampler{nullptr}; }
    vk::ImageView view() const { return _adoptedView ? _adoptedView : (_view != nullptr ? *_view : vk::ImageView{nullptr}); }
    vk::ImageLayout layout() const { return _layout; }
    vk::Extent2D extent() const { return _extent; }
    vk::Format format() const { return _format; }
    uint32_t mipLevels() const { return _mipLevels; }
    TextureFilter minFilter() const { return _minFilter; }

private:
    bool createImage(const TextureDesc& desc, int width, int height, uint32_t mipLevels,
                     vk::ImageUsageFlags usage);
    bool createSampler(const TextureDesc& desc);
    bool createView(vk::ImageAspectFlags aspect);
    bool genMipmaps(int width, int height);

    vk::raii::Device& _dev;
    vk::raii::PhysicalDevice& _phys;
    vk::raii::Queue& _queue;
    uint32_t _graphicsFamily{0};

    vk::Format _format{vk::Format::eR8G8B8A8Unorm};
    vk::Extent2D _extent{};
    uint32_t _mipLevels{1};
    TextureFilter _minFilter{TextureFilter::LinearMipLinear};
    bool _adopted{false};
    bool _valid{false};
    vk::ImageView _adoptedView{VK_NULL_HANDLE};
    vk::ImageLayout _layout{vk::ImageLayout::eShaderReadOnlyOptimal};

    vk::raii::Image _image{nullptr};
    vk::raii::DeviceMemory _memory{nullptr};
    vk::raii::ImageView _view{nullptr};
    vk::raii::Sampler _sampler{nullptr};
};

} // namespace rhi
