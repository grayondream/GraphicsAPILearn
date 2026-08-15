#include "VKSwapchain.hpp"
#include "VKHeader.hpp"
#include "base/Log.hpp"
#include <algorithm>
#include <limits>

namespace rhi {

bool VKSwapchain::init(vk::raii::Device& device, vk::raii::PhysicalDevice& phys,
                       vk::raii::SurfaceKHR& surface, vk::raii::Queue& presentQueue,
                       int width, int height) {
    _device = &device;
    _phys = &phys;
    _surface = &surface;
    _presentQueue = &presentQueue;
    return createSwapchain(width, height);
}

void VKSwapchain::shutdown() {
    _views.clear();
    _swapchain = vk::raii::SwapchainKHR{nullptr};
    _images.clear();
    _initialized = false;
}

vk::SurfaceFormatKHR VKSwapchain::pickFormat() {
    auto rf = _phys->getSurfaceFormatsKHR(static_cast<vk::SurfaceKHR>(**_surface));
    if (rf.result == vk::Result::eSuccess && !rf.value.empty()) {
        for (const auto& f : rf.value) {
            if (f.format == vk::Format::eB8G8R8A8Unorm && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                return f;
        }
        for (const auto& f : rf.value) {
            if (f.format == vk::Format::eR8G8B8A8Unorm && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                return f;
        }
        return rf.value[0];
    }
    return vk::SurfaceFormatKHR{vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear};
}

vk::PresentModeKHR VKSwapchain::pickPresentMode() {
    vk::PresentModeKHR best = vk::PresentModeKHR::eFifo;
    auto rm = _phys->getSurfacePresentModesKHR(static_cast<vk::SurfaceKHR>(**_surface));
    if (rm.result == vk::Result::eSuccess) {
        for (const auto& m : rm.value) {
            if (m == vk::PresentModeKHR::eMailbox) return m;
            if (m == vk::PresentModeKHR::eFifo) best = m;
        }
    }
    return best;
}

vk::Extent2D VKSwapchain::pickExtent(int width, int height, const vk::SurfaceCapabilitiesKHR& caps) {
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return caps.currentExtent;
    }
    vk::Extent2D ext(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    ext.width = std::clamp(ext.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    ext.height = std::clamp(ext.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return ext;
}

bool VKSwapchain::createSwapchain(int width, int height) {
    auto cr = _phys->getSurfaceCapabilitiesKHR(static_cast<vk::SurfaceKHR>(**_surface));
    if (cr.result != vk::Result::eSuccess) {
        LOGE("VKSwapchain: getSurfaceCapabilitiesKHR failed");
        return false;
    }
    const vk::SurfaceCapabilitiesKHR caps = cr.value;
    const vk::SurfaceFormatKHR format = pickFormat();
    const vk::PresentModeKHR mode = pickPresentMode();
    const vk::Extent2D ext = pickExtent(width, height, caps);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;
    if (imageCount < 3) imageCount = 3;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

    vk::SwapchainCreateInfoKHR ci{};
    ci.surface = static_cast<vk::SurfaceKHR>(**_surface);
    ci.minImageCount = imageCount;
    ci.imageFormat = format.format;
    ci.imageColorSpace = format.colorSpace;
    ci.imageExtent = ext;
    ci.imageArrayLayers = 1;
    ci.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
    ci.imageSharingMode = vk::SharingMode::eExclusive;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    ci.presentMode = mode;
    ci.clipped = vk::True;

    auto sr = _device->createSwapchainKHR(ci);
    if (sr.result != vk::Result::eSuccess) {
        LOGE("VKSwapchain: createSwapchainKHR failed");
        return false;
    }
    _swapchain = std::move(sr.value);

    auto ir = _swapchain.getImages();
    if (ir.result != vk::Result::eSuccess) {
        LOGE("VKSwapchain: getImages failed");
        return false;
    }
    _images = std::move(ir.value);
    _format = format.format;
    _extent = ext;
    _initialized = true;
    return createImageViews();
}

bool VKSwapchain::createImageViews() {
    _views.clear();
    for (const auto& img : _images) {
        vk::ImageViewCreateInfo ci{};
        ci.image = img;
        ci.viewType = vk::ImageViewType::e2D;
        ci.format = _format;
        ci.components.r = vk::ComponentSwizzle::eIdentity;
        ci.components.g = vk::ComponentSwizzle::eIdentity;
        ci.components.b = vk::ComponentSwizzle::eIdentity;
        ci.components.a = vk::ComponentSwizzle::eIdentity;
        ci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        ci.subresourceRange.baseMipLevel = 0;
        ci.subresourceRange.levelCount = 1;
        ci.subresourceRange.baseArrayLayer = 0;
        ci.subresourceRange.layerCount = 1;
        auto vr = _device->createImageView(ci);
        if (vr.result != vk::Result::eSuccess) {
            LOGE("VKSwapchain: createImageView failed");
            return false;
        }
        _views.push_back(std::move(vr.value));
    }
    return true;
}

bool VKSwapchain::acquire(vk::Semaphore imageReady) {
    auto r = _swapchain.acquireNextImage(std::numeric_limits<uint64_t>::max(), imageReady, nullptr);
    if (r.result == vk::Result::eSuccess || r.result == vk::Result::eSuboptimalKHR) {
        _imageIndex = r.value;
        return true;
    }
    return false;
}

bool VKSwapchain::present() {
    vk::SwapchainKHR sc = static_cast<vk::SwapchainKHR>(*_swapchain);
    vk::PresentInfoKHR info{};
    if (_presentSemaphore) {
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &_presentSemaphore;
    }
    info.swapchainCount = 1;
    info.pSwapchains = &sc;
    info.pImageIndices = &_imageIndex;
    vk::Result r = _presentQueue->presentKHR(info);
    return r == vk::Result::eSuccess || r == vk::Result::eSuboptimalKHR;
}

void VKSwapchain::resize(int width, int height) {
    if (!_initialized) return;
    _views.clear();
    _swapchain = vk::raii::SwapchainKHR{nullptr};
    createSwapchain(width, height);
}

void* VKSwapchain::handle() {
    return reinterpret_cast<void*>(static_cast<VkSwapchainKHR>(*_swapchain));
}

} // namespace rhi
