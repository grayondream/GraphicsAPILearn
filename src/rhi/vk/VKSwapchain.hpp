#pragma once
#include "VKHeader.hpp"
#include "rhi/core/ISwapchain.hpp"

namespace rhi {

class VKSwapchain : public ISwapchain {
public:
    ~VKSwapchain() override = default;

    bool init(vk::raii::Device& device, vk::raii::PhysicalDevice& phys,
              vk::raii::SurfaceKHR& surface, vk::raii::Queue& presentQueue,
              int width, int height);
    void shutdown();

    bool present() override;
    void resize(int width, int height) override;
    void* handle() override;

    bool acquire(vk::Semaphore imageReady);
    void setPresentSemaphore(vk::Semaphore semaphore) { _presentSemaphore = semaphore; }
    uint32_t imageCount() const { return static_cast<uint32_t>(_images.size()); }
    uint32_t currentImage() const { return _imageIndex; }
    vk::ImageView imageView(uint32_t index) const { return *_views[index]; }
    vk::Image image(uint32_t index) const { return _images[index]; }
    vk::Extent2D extent() const { return _extent; }
    vk::Format format() const { return _format; }

private:
    bool createSwapchain(int width, int height);
    bool createImageViews();
    vk::SurfaceFormatKHR pickFormat();
    vk::PresentModeKHR pickPresentMode();
    vk::Extent2D pickExtent(int width, int height, const vk::SurfaceCapabilitiesKHR& caps);

    vk::raii::Device* _device{nullptr};
    vk::raii::PhysicalDevice* _phys{nullptr};
    vk::raii::SurfaceKHR* _surface{nullptr};
    vk::raii::Queue* _presentQueue{nullptr};
    vk::raii::SwapchainKHR _swapchain{nullptr};
    std::vector<vk::Image> _images{};
    std::vector<vk::raii::ImageView> _views{};
    vk::Format _format{vk::Format::eB8G8R8A8Unorm};
    vk::Extent2D _extent{};
    uint32_t _imageIndex{0};
    vk::Semaphore _presentSemaphore{};
    bool _initialized{false};
};

} // namespace rhi
