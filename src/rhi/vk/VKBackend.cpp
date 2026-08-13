#include "VKBackend.hpp"
#include "VKHeader.hpp"
#include "VKSwapchain.hpp"
#include <GLFW/glfw3.h>
#include "rhi/core/ISurface.hpp"
#include "base/Log.hpp"
#include <cstring>
#include <vector>
#include <memory>

namespace rhi {

struct QueueFamilies {
    uint32_t graphics{0};
    uint32_t present{0};
    bool found{false};
};

class VKRenderer final : public IRenderer {
public:
    VKRenderer() = default;
    ~VKRenderer() override { shutdown(); }

    bool init(const std::shared_ptr<ISurface>& surface) override;
    void shutdown() override;

    void beginFrame() override { if (_swapchain) _swapchain->acquire(); }
    void endFrame() override {}
    bool present() override { return _swapchain ? _swapchain->present() : false; }

    std::shared_ptr<IShader> createShader() override { return {}; }
    std::shared_ptr<IPipeline> createPipeline(const VertexLayout&, const std::shared_ptr<IShader>&) override { return {}; }
    std::shared_ptr<IBuffer> createBuffer() override { return {}; }
    std::shared_ptr<IBuffer> createUniformBuffer() override { return {}; }
    std::shared_ptr<ITexture2D> createTexture2D() override { return {}; }
    std::shared_ptr<ITexture3D> createTexture3D() override { return {}; }
    std::shared_ptr<IRenderTarget> createRenderTarget() override { return {}; }
    std::shared_ptr<ISwapchain> getSwapchain() override { return _swapchain; }

    void clearColor(float, float, float, float) override {}
    void setViewport(const Viewport&) override {}
    void setPipeline(const std::shared_ptr<IPipeline>&) override {}
    void setVertexBuffer(const std::shared_ptr<IBuffer>&) override {}
    void setVertexBuffer(const std::shared_ptr<IBuffer>&, uint32_t) override {}
    void setIndexBuffer(const std::shared_ptr<IBuffer>&) override {}
    void setRenderTarget(const std::shared_ptr<IRenderTarget>&) override {}
    void bindTexture(const std::shared_ptr<ITexture2D>&, unsigned int) override {}
    void bindTexture(const std::shared_ptr<ITexture3D>&, unsigned int) override {}
    void bindTexture(rhi::ITexture2D*, unsigned int) override {}
    void draw(uint32_t, uint32_t) override {}
    void drawIndexed(uint32_t, uint32_t, uint32_t) override {}
    void drawIndexedInstanced(uint32_t, uint32_t, uint32_t, uint32_t) override {}
    void drawInstanced(uint32_t, uint32_t, uint32_t) override {}
    void blitFramebuffer(const std::shared_ptr<IRenderTarget>&, const std::shared_ptr<IRenderTarget>&, BlitMask) override {}
    BackendCapabilities backendCapabilities() override {
        BackendCapabilities caps;
        caps.maxSamples = 8;
        caps.maxUniformBlockSize = 16384;
        return caps;
    }

private:
    bool pickPhysicalDevice();
    bool isDeviceSuitable(vk::raii::PhysicalDevice& pd);
    QueueFamilies findQueueFamilies(vk::raii::PhysicalDevice& pd);
    bool createDevice(const QueueFamilies& families);

    std::shared_ptr<ISurface> _surface{};
    std::shared_ptr<VKSwapchain> _swapchain{};
    vk::raii::Context _ctx{};
    vk::raii::Instance _instance{nullptr};
    vk::raii::PhysicalDevice _phys{nullptr};
    vk::raii::SurfaceKHR _surfaceKHR{nullptr};
    vk::raii::Device _device{nullptr};
    vk::raii::Queue _graphicsQueue{nullptr};
    vk::raii::Queue _presentQueue{nullptr};
    uint32_t _graphicsFamily{0};
    uint32_t _presentFamily{0};
};

bool VKRenderer::init(const std::shared_ptr<ISurface>& surface) {
    shutdown();
    _surface = surface;

    uint32_t extCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&extCount);
    if (!glfwExts) {
        LOGE("VKRenderer: glfwGetRequiredInstanceExtensions failed");
        return false;
    }
    std::vector<const char*> extensions(glfwExts, glfwExts + extCount);

    vk::ApplicationInfo appInfo{};
    appInfo.pApplicationName = "renderLearn";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "renderLearn";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    vk::InstanceCreateInfo ici{};
    ici.pApplicationInfo = &appInfo;
    ici.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    ici.ppEnabledExtensionNames = extensions.data();

    auto ir = _ctx.createInstance(ici);
    if (ir.result != vk::Result::eSuccess) {
        LOGE("VKRenderer: createInstance failed");
        return false;
    }
    _instance = std::move(ir.value);

    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(static_cast<VkInstance>(*_instance),
                                static_cast<GLFWwindow*>(_surface->nativeHandle()),
                                nullptr, &rawSurface) != VK_SUCCESS) {
        LOGE("VKRenderer: glfwCreateWindowSurface failed");
        return false;
    }
    _surfaceKHR = vk::raii::SurfaceKHR(_instance, rawSurface);

    if (!pickPhysicalDevice()) return false;
    QueueFamilies families = findQueueFamilies(_phys);
    if (!families.found) {
        LOGE("VKRenderer: no queue families support graphics+present");
        return false;
    }
    _graphicsFamily = families.graphics;
    _presentFamily = families.present;
    if (!createDevice(families)) return false;

    _swapchain = std::make_shared<VKSwapchain>();
    if (!_swapchain->init(_device, _phys, _surfaceKHR, _presentQueue,
                          _surface->width(), _surface->height())) {
        LOGE("VKRenderer: swapchain init failed");
        return false;
    }
    return true;
}

void VKRenderer::shutdown() {
    if (_device != nullptr) {
        auto w = _device.waitIdle();
        (void)w;
    }
    _swapchain.reset();
    _presentQueue = vk::raii::Queue{nullptr};
    _graphicsQueue = vk::raii::Queue{nullptr};
    _device = vk::raii::Device{nullptr};
    _surfaceKHR = vk::raii::SurfaceKHR{nullptr};
    _phys = vk::raii::PhysicalDevice{nullptr};
    _instance = vk::raii::Instance{nullptr};
    _surface.reset();
}

bool VKRenderer::isDeviceSuitable(vk::raii::PhysicalDevice& pd) {
    QueueFamilies qf = findQueueFamilies(pd);
    if (!qf.found) return false;

    bool swapchainExt = false;
    auto exts = pd.enumerateDeviceExtensionProperties();
    if (exts.result == vk::Result::eSuccess) {
        for (const auto& e : exts.value) {
            if (std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                swapchainExt = true;
                break;
            }
        }
    }
    if (!swapchainExt) return false;

    auto fmts = pd.getSurfaceFormatsKHR(*_surfaceKHR);
    auto modes = pd.getSurfacePresentModesKHR(*_surfaceKHR);
    return fmts.result == vk::Result::eSuccess && !fmts.value.empty() &&
           modes.result == vk::Result::eSuccess && !modes.value.empty();
}

bool VKRenderer::pickPhysicalDevice() {
    auto pr = _instance.enumeratePhysicalDevices();
    if (pr.result != vk::Result::eSuccess || pr.value.empty()) {
        LOGE("VKRenderer: no physical devices found");
        return false;
    }
    int best = -1;
    int bestScore = -1;
    for (size_t i = 0; i < pr.value.size(); i++) {
        vk::raii::PhysicalDevice& pd = pr.value[i];
        if (!isDeviceSuitable(pd)) continue;
        const vk::PhysicalDeviceProperties props = pd.getProperties();
        int score = 0;
        if (props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) score += 1000;
        else if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) score += 500;
        if (score > bestScore) {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    if (best < 0) {
        LOGE("VKRenderer: no suitable physical device");
        return false;
    }
    _phys = std::move(pr.value[static_cast<size_t>(best)]);
    return true;
}

QueueFamilies VKRenderer::findQueueFamilies(vk::raii::PhysicalDevice& pd) {
    QueueFamilies qf;
    const vk::SurfaceKHR surf = *_surfaceKHR;
    const auto props = pd.getQueueFamilyProperties();
    bool gFound = false;
    bool pFound = false;
    for (uint32_t i = 0; i < props.size(); i++) {
        const bool gfx = (props[i].queueFlags & vk::QueueFlagBits::eGraphics) == vk::QueueFlagBits::eGraphics;
        bool present = false;
        auto sp = pd.getSurfaceSupportKHR(i, surf);
        if (sp.result == vk::Result::eSuccess) present = (sp.value != vk::False);
        if (gfx) { qf.graphics = i; gFound = true; }
        if (present) { qf.present = i; pFound = true; }
    }
    qf.found = gFound && pFound;
    return qf;
}

bool VKRenderer::createDevice(const QueueFamilies& families) {
    const float priority = 1.0f;
    uint32_t familiesArr[2] = { families.graphics, families.present };
    const uint32_t uniqueCount = (families.graphics == families.present) ? 1 : 2;

    std::vector<vk::DeviceQueueCreateInfo> queueInfos;
    for (uint32_t i = 0; i < uniqueCount; i++) {
        vk::DeviceQueueCreateInfo qi{};
        qi.queueFamilyIndex = familiesArr[i];
        qi.queueCount = 1;
        qi.pQueuePriorities = &priority;
        queueInfos.push_back(qi);
    }

    const char* deviceExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    vk::DeviceCreateInfo dci{};
    dci.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    dci.pQueueCreateInfos = queueInfos.data();
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = deviceExts;

    auto dr = _phys.createDevice(dci);
    if (dr.result != vk::Result::eSuccess) {
        LOGE("VKRenderer: createDevice failed");
        return false;
    }
    _device = std::move(dr.value);
    _graphicsQueue = _device.getQueue(families.graphics, 0);
    _presentQueue = _device.getQueue(families.present, 0);
    return true;
}

std::shared_ptr<IRenderer> createVKRenderer() {
    return std::make_shared<VKRenderer>();
}

} // namespace rhi
