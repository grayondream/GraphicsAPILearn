#pragma once
#include "VKHeader.hpp"
#include "rhi/core/ITexture3D.hpp"
#include <algorithm>
#include <array>

namespace rhi {

class VKTexture3D : public ITexture3D {
public:
    VKTexture3D(vk::raii::Device& device, vk::raii::PhysicalDevice& phys,
                vk::raii::Queue& queue, uint32_t graphicsFamily)
        : _dev(device), _phys(phys), _queue(queue), _graphicsFamily(graphicsFamily) {}

    bool init(const TextureDataView3D& data) override;
    bool initCube(const TextureDesc& desc, const TextureDataView2D* faces) override;
    bool createEmpty(const TextureDesc& desc, int width, int height) override;
    void bind(unsigned int unit) override;
    void* handle() override;
    bool valid() const override { return _valid; }
    void release() override;
    void genCubeMipmaps() override;

    vk::Sampler sampler() const { return _sampler != nullptr ? *_sampler : vk::Sampler{nullptr}; }
    vk::ImageView cubeView() const { return _cubeView != nullptr ? *_cubeView : vk::ImageView{nullptr}; }
    vk::ImageView depthCubeView() const { return _depthCubeView != nullptr ? *_depthCubeView : vk::ImageView{nullptr}; }
    vk::ImageView faceView(int face, int mip) const;
    vk::Image image() const { return _image != nullptr ? *_image : vk::Image{nullptr}; }
    vk::ImageLayout layout() const { return _layout; }
    vk::Extent2D extent() const { return _extent; }
    vk::Extent2D mipExtent(uint32_t mip) const {
        return vk::Extent2D(std::max(1u, _extent.width >> mip), std::max(1u, _extent.height >> mip));
    }
    vk::Format format() const { return _format; }
    uint32_t mipLevels() const { return _mipLevels; }
    bool isCube() const { return _cube; }
    bool isDepth() const { return _depth; }
    void debugDumpCubeFace(const char* path, int face, int mip = 0) const;

private:
    bool createCubeImage(int width, int height, uint32_t mipLevels, vk::ImageUsageFlags usage);
    bool createCubeViews(vk::ImageAspectFlags aspect);
    bool createSampler(const TextureDesc& desc);

    vk::raii::Device& _dev;
    vk::raii::PhysicalDevice& _phys;
    vk::raii::Queue& _queue;
    uint32_t _graphicsFamily{0};

    vk::Format _format{vk::Format::eR8G8B8A8Unorm};
    vk::Extent2D _extent{};
    uint32_t _mipLevels{1};
    bool _cube{false};
    bool _depth{false};
    bool _valid{false};
    vk::ImageLayout _layout{vk::ImageLayout::eShaderReadOnlyOptimal};

    vk::raii::Image _image{nullptr};
    vk::raii::DeviceMemory _memory{nullptr};
    vk::raii::ImageView _cubeView{nullptr};
    vk::raii::ImageView _depthCubeView{nullptr};
    std::vector<vk::raii::ImageView> _faceViews{};
    vk::raii::Sampler _sampler{nullptr};
};

} // namespace rhi
