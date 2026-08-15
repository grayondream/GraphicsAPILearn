#include "VKBackend.hpp"
#include "VKHeader.hpp"
#include "VKFormat.hpp"
#include "VKSwapchain.hpp"
#include "VKBuffer.hpp"
#include "VKShader.hpp"
#include "VKPipeline.hpp"
#include "VKTexture2D.hpp"
#include "VKTexture3D.hpp"
#include "VKRenderTarget.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include "rhi/core/ISurface.hpp"
#include "base/Log.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <vector>
#include <memory>

namespace rhi {

struct QueueFamilies {
    uint32_t graphics{0};
    uint32_t present{0};
    bool found{false};
};

class VKRenderer final : public IRenderer, public VKBuffer::Notifier {
public:
    static constexpr size_t kUboSlots = 32;
    using DescriptorSet = vk::raii::DescriptorSet;
    VKRenderer() = default;
    ~VKRenderer() override { shutdown(); }

    bool init(const std::shared_ptr<ISurface>& surface) override;
    void shutdown() override;

    void beginFrame() override;
    void endFrame() override;
    bool present() override;

    std::shared_ptr<IShader> createShader() override;
    std::shared_ptr<IPipeline> createPipeline(const VertexLayout& layout, const std::shared_ptr<IShader>& shader) override;
    std::shared_ptr<IBuffer> createBuffer() override;
    std::shared_ptr<IBuffer> createUniformBuffer() override;
    std::shared_ptr<ITexture2D> createTexture2D() override;
    std::shared_ptr<ITexture3D> createTexture3D() override;
    std::shared_ptr<IRenderTarget> createRenderTarget() override;
    std::shared_ptr<ISwapchain> getSwapchain() override;

    void clearColor(float r, float g, float b, float a) override;
    void setViewport(const Viewport& vp) override;
    void setPipeline(const std::shared_ptr<IPipeline>& pipeline) override;
    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer) override;
    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer, uint32_t binding) override;
    void setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) override;
    void setRenderTarget(const std::shared_ptr<IRenderTarget>&) override;
    void bindTexture(const std::shared_ptr<ITexture2D>&, unsigned int) override;
    void bindTexture(const std::shared_ptr<ITexture3D>&, unsigned int) override;
    void bindTexture(rhi::ITexture2D*, unsigned int) override;
    void draw(uint32_t vertexCount, uint32_t firstVertex) override;
    void drawIndexed(uint32_t indexCount, uint32_t indexOffset, uint32_t vertexOffset) override;
    void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t indexOffset, uint32_t vertexOffset) override;
    void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) override;
    void blitFramebuffer(const std::shared_ptr<IRenderTarget>&, const std::shared_ptr<IRenderTarget>&, BlitMask) override;
    BackendCapabilities backendCapabilities() override;

    bool imguiInitInfo(VKImGuiInitInfo& out) override;
    void renderImGuiDrawData(void* drawData) override;

    void onUniformCreated(VKBuffer* buffer, size_t offset, size_t size) override;
    void onUniformUpdated(VKBuffer* buffer, uint32_t slot, size_t offset, size_t size) override;

private:
    bool pickPhysicalDevice();
    bool isDeviceSuitable(vk::raii::PhysicalDevice& pd);
    QueueFamilies findQueueFamilies(vk::raii::PhysicalDevice& pd);
    bool createDevice(const QueueFamilies& families);
    bool createDescriptors();
    bool createRenderPassAndFramebuffers();
    bool createCommandResources();
    void updateUboDescriptor();
    void updateSamplerDescriptor(uint32_t binding, vk::Sampler sampler, vk::ImageView view);
    bool ensureRenderPass();
    void applyViewport();
    bool bindPipelineAndState();
    bool bindVertexBuffers();
    void dumpFrame();
    vk::Extent2D extent() const { return _swapchain ? _swapchain->extent() : vk::Extent2D{}; }

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

    vk::raii::DescriptorSetLayout _dsLayout{nullptr};
    vk::raii::DescriptorPool _dsPool{nullptr};
    std::vector<vk::raii::DescriptorSet> _uboDs{};
    vk::raii::PipelineLayout _pipelineLayout{nullptr};
    vk::raii::RenderPass _renderPass{nullptr};
    std::vector<vk::raii::Framebuffer> _framebuffers{};
    vk::raii::CommandPool _cmdPool{nullptr};
    vk::raii::CommandBuffer _cmd{nullptr};
    vk::raii::CommandPool _dumpPool{nullptr};
    vk::raii::CommandBuffer _dumpCmd{nullptr};
    vk::raii::Buffer _dumpBuffer{nullptr};
    vk::raii::DeviceMemory _dumpMemory{nullptr};
    vk::raii::Fence _dumpFence{nullptr};
    std::vector<vk::Image> _dumpImages{};
    bool _dumpDone{false};
    vk::raii::Semaphore _imageReady{nullptr};
    vk::raii::Semaphore _rendered{nullptr};
    vk::raii::Fence _frameFence{nullptr};

    bool _recording{false};
    bool _rpActive{false};
    bool _viewportSet{false};
    bool _floatRtFallback{false};
    Viewport _viewport{};
    float _clearColor[4]{0.0f, 0.0f, 0.0f, 1.0f};
    std::shared_ptr<IPipeline> _pipeline{};
    std::array<std::shared_ptr<IBuffer>, 16> _vertexBuffers{};
    std::shared_ptr<IBuffer> _indexBuffer{};
    VKBuffer* _uboBuffer{nullptr};
    size_t _uboSlotOffset{0};
    size_t _uboSlotSize{0};
    uint32_t _uboSlotIndex{0};
    std::shared_ptr<IRenderTarget> _renderTarget{};
    std::shared_ptr<VKRenderTarget> _vkRenderTarget{};
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
    // llvmpipe(软件渲染) 下 float 离屏 RT 无法可靠写入(HDR 场景整帧黑屏/崩溃)，
    // 降级为 RGBA8 保证画面；真机 GPU 不受影响。
    const std::string devName = _phys.getProperties().deviceName;
    if (devName.find("llvmpipe") != std::string::npos) _floatRtFallback = true;
    LOGI("VKRenderer: device={} floatRtFallback={}", devName, _floatRtFallback);
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
    if (!createDescriptors()) return false;
    if (!createRenderPassAndFramebuffers()) return false;
    if (!createCommandResources()) return false;
    return true;
}

bool VKRenderer::createDescriptors() {
    std::vector<vk::DescriptorSetLayoutBinding> dsb;
    dsb.push_back(vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment));
    for (int i = 1; i <= 15; i++) {
        dsb.push_back(vk::DescriptorSetLayoutBinding(static_cast<uint32_t>(i),
            vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment));
    }
    vk::DescriptorSetLayoutCreateInfo dslci{};
    dslci.bindingCount = static_cast<uint32_t>(dsb.size());
    dslci.pBindings = dsb.data();
    auto dlr = _device.createDescriptorSetLayout(dslci);
    if (dlr.result != vk::Result::eSuccess) {
        LOGE("VKRenderer: createDescriptorSetLayout failed");
        return false;
    }
    _dsLayout = std::move(dlr.value);

    vk::DescriptorPoolSize sizes[] = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 255),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 255 * 15),
    };
    vk::DescriptorPoolCreateInfo dpci{};
    dpci.maxSets = 255;
    dpci.poolSizeCount = 2;
    dpci.pPoolSizes = sizes;
    auto dpr = _device.createDescriptorPool(dpci);
    if (dpr.result != vk::Result::eSuccess) {
        LOGE("VKRenderer: createDescriptorPool failed");
        return false;
    }
    _dsPool = std::move(dpr.value);

    // 为 UBO ring 的每个 slot 预分配一个独立 descriptor set：multi-pass App 在
    // 同一 command buffer 内多次 update 同一 UBO，每个 pass 的 draw 必须绑到各自
    // 槽对应的 set，否则 GPU 读到的总是最后一次覆盖的偏移（整帧黑）。
    const uint32_t setCount = kUboSlots;
    std::vector<vk::DescriptorSetLayout> lay(setCount, * _dsLayout);
    vk::DescriptorSetAllocateInfo dsai(*_dsPool, static_cast<uint32_t>(lay.size()), lay.data());
    auto dar = _device.allocateDescriptorSets(dsai);
    if (dar.result != vk::Result::eSuccess) {
        LOGE("VKRenderer: allocateDescriptorSets failed");
        return false;
    }
    _uboDs.clear();
    for (auto& v : dar.value) _uboDs.emplace_back(std::move(v));

    vk::PipelineLayoutCreateInfo plci{};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &*_dsLayout;
    auto plr = _device.createPipelineLayout(plci);
    if (plr.result != vk::Result::eSuccess) {
        LOGE("VKRenderer: createPipelineLayout failed");
        return false;
    }
    _pipelineLayout = std::move(plr.value);
    return true;
}

bool VKRenderer::createRenderPassAndFramebuffers() {
    const vk::Format fmt = _swapchain->format();
    vk::AttachmentDescription color{};
    color.format = fmt;
    color.samples = vk::SampleCountFlagBits::e1;
    color.loadOp = vk::AttachmentLoadOp::eClear;
    color.storeOp = vk::AttachmentStoreOp::eStore;
    color.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color.initialLayout = vk::ImageLayout::eUndefined;
    color.finalLayout = vk::ImageLayout::ePresentSrcKHR;
    vk::AttachmentReference colorRef(0, vk::ImageLayout::eColorAttachmentOptimal);
    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    vk::RenderPassCreateInfo rpci{};
    rpci.attachmentCount = 1;
    rpci.pAttachments = &color;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    auto rpr = _device.createRenderPass(rpci);
    if (rpr.result != vk::Result::eSuccess) {
        LOGE("VKRenderer: createRenderPass failed");
        return false;
    }
    _renderPass = std::move(rpr.value);

    _framebuffers.clear();
    const vk::Extent2D ext = _swapchain->extent();
    for (uint32_t i = 0; i < _swapchain->imageCount(); i++) {
        vk::ImageView view = _swapchain->imageView(i);
        vk::FramebufferCreateInfo fci{};
        fci.renderPass = *_renderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &view;
        fci.width = ext.width;
        fci.height = ext.height;
        fci.layers = 1;
        auto fr = _device.createFramebuffer(fci);
        if (fr.result != vk::Result::eSuccess) {
            LOGE("VKRenderer: createFramebuffer failed");
            return false;
        }
        _framebuffers.push_back(std::move(fr.value));
    }
    return true;
}

bool VKRenderer::createCommandResources() {
    vk::CommandPoolCreateInfo cpci{};
    cpci.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    cpci.queueFamilyIndex = _graphicsFamily;
    auto cpr = _device.createCommandPool(cpci);
    if (cpr.result != vk::Result::eSuccess) {
        LOGE("VKRenderer: createCommandPool failed");
        return false;
    }
    _cmdPool = std::move(cpr.value);

    vk::CommandBufferAllocateInfo cba(*_cmdPool, vk::CommandBufferLevel::ePrimary, 1);
    auto cbr = _device.allocateCommandBuffers(cba);
    if (cbr.result != vk::Result::eSuccess) {
        LOGE("VKRenderer: allocateCommandBuffers failed");
        return false;
    }
    _cmd = std::move(cbr.value[0]);

    auto sr = _device.createSemaphore(vk::SemaphoreCreateInfo{});
    if (sr.result != vk::Result::eSuccess) return false;
    _imageReady = std::move(sr.value);
    sr = _device.createSemaphore(vk::SemaphoreCreateInfo{});
    if (sr.result != vk::Result::eSuccess) return false;
    _rendered = std::move(sr.value);

    vk::FenceCreateInfo fci(vk::FenceCreateFlagBits::eSignaled);
    auto fr = _device.createFence(fci);
    if (fr.result != vk::Result::eSuccess) {
        LOGE("VKRenderer: createFence failed");
        return false;
    }
    _frameFence = std::move(fr.value);
    return true;
}

void VKRenderer::shutdown() {
    if (_device != nullptr) {
        _device.waitIdle();
    }
    _swapchain.reset();
    _framebuffers.clear();
    _renderPass = vk::raii::RenderPass{nullptr};
    _pipelineLayout = vk::raii::PipelineLayout{nullptr};
    _uboDs.clear();
    _dsPool = vk::raii::DescriptorPool{nullptr};
    _dsLayout = vk::raii::DescriptorSetLayout{nullptr};
    _cmd = vk::raii::CommandBuffer{nullptr};
    _cmdPool = vk::raii::CommandPool{nullptr};
    _frameFence = vk::raii::Fence{nullptr};
    _rendered = vk::raii::Semaphore{nullptr};
    _imageReady = vk::raii::Semaphore{nullptr};
    _presentQueue = vk::raii::Queue{nullptr};
    _graphicsQueue = vk::raii::Queue{nullptr};
    _device = vk::raii::Device{nullptr};
    _surfaceKHR = vk::raii::SurfaceKHR{nullptr};
    _phys = vk::raii::PhysicalDevice{nullptr};
    _instance = vk::raii::Instance{nullptr};
    _surface.reset();
    _pipeline.reset();
    _indexBuffer.reset();
    _vertexBuffers = {};
    _renderTarget.reset();
    _vkRenderTarget.reset();
    // _uboBuffer is a raw (non-owning) pointer to the App's UBO buffer. The App
    // must keep its _uboBuffer alive for as long as this renderer uses it; VK
    // only registers it (GL mode keeps its own buffer). Clear it here so we
    // never dereference a dangling pointer if the App outlives shutdown.
    _uboBuffer = nullptr;
    _recording = false;
    _rpActive = false;
}

std::shared_ptr<IShader> VKRenderer::createShader() {
    return std::make_shared<VKShader>(_device);
}

std::shared_ptr<IPipeline> VKRenderer::createPipeline(const VertexLayout& layout, const std::shared_ptr<IShader>& shader) {
    auto vks = std::dynamic_pointer_cast<VKShader>(shader);
    return std::make_shared<VKPipeline>(_device, *_pipelineLayout, layout, vks);
}

std::shared_ptr<IBuffer> VKRenderer::createBuffer() {
    return std::make_shared<VKBuffer>(_device, _phys, _graphicsQueue, _graphicsFamily);
}

std::shared_ptr<IBuffer> VKRenderer::createUniformBuffer() {
    auto buf = std::make_shared<VKBuffer>(_device, _phys, _graphicsQueue, _graphicsFamily);
    buf->setNotifier(this);
    return buf;
}

std::shared_ptr<ITexture2D> VKRenderer::createTexture2D() {
    return std::make_shared<VKTexture2D>(_device, _phys, _graphicsQueue, _graphicsFamily);
}
std::shared_ptr<ITexture3D> VKRenderer::createTexture3D() {
    return std::make_shared<VKTexture3D>(_device, _phys, _graphicsQueue, _graphicsFamily);
}
std::shared_ptr<IRenderTarget> VKRenderer::createRenderTarget() {
    return std::make_shared<VKRenderTarget>(_device, _phys, _graphicsQueue, _graphicsFamily, _floatRtFallback);
}
std::shared_ptr<ISwapchain> VKRenderer::getSwapchain() { return _swapchain; }

void VKRenderer::beginFrame() {
    if (_recording || !_swapchain) return;
    (void)_device.waitForFences({*_frameFence}, vk::True, UINT64_MAX);
    _device.resetFences({*_frameFence});
    if (!_swapchain->acquire(static_cast<vk::Semaphore>(*_imageReady))) return;

    vk::CommandBufferBeginInfo cbbi(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    vk::Result br = _cmd.begin(cbbi);
    if (br != vk::Result::eSuccess) return;
    _recording = true;
    _rpActive = false;
}

void VKRenderer::endFrame() {
    if (!_recording) return;
    if (_rpActive) _cmd.endRenderPass();
    vk::Result er = _cmd.end();
    _recording = false;
    if (er != vk::Result::eSuccess) return;

    vk::Semaphore waitSem = *_imageReady;
    vk::Semaphore signalSem = *_rendered;
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::CommandBuffer cb = *_cmd;
    vk::SubmitInfo si{};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &waitSem;
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &signalSem;
    _graphicsQueue.submit({si}, *_frameFence);
}

bool VKRenderer::present() {
    if (!_swapchain) return false;
    dumpFrame();
    _swapchain->setPresentSemaphore(static_cast<vk::Semaphore>(*_rendered));
    return _swapchain->present();
}

void VKRenderer::dumpFrame() {
    const char* dumpPath = std::getenv("RHI_DUMP_FRAME");
    if (!dumpPath || _dumpDone || !_swapchain) return;
    _dumpDone = true;

    const vk::Extent2D ext = _swapchain->extent();
    const uint32_t idx = _swapchain->currentImage();
    const bool f16Swap = (_swapchain->format() == vk::Format::eR16G16B16A16Sfloat);
    const uint32_t fbpp = f16Swap ? 8u : 4u;
    const uint64_t pixelBytes = static_cast<uint64_t>(ext.width) * ext.height * fbpp;

    _device.waitIdle();

    vk::BufferCreateInfo bci{};
    bci.size = pixelBytes;
    bci.usage = vk::BufferUsageFlagBits::eTransferDst;
    bci.sharingMode = vk::SharingMode::eExclusive;
    auto br = _device.createBuffer(bci);
    if (br.result != vk::Result::eSuccess) { LOGE("dumpFrame: createBuffer failed"); return; }
    _dumpBuffer = std::move(br.value);

    vk::MemoryRequirements mreq = _dumpBuffer.getMemoryRequirements();
    uint32_t memIdx = UINT32_MAX;
    vk::PhysicalDeviceMemoryProperties memProps = _phys.getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((mreq.memoryTypeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) &&
            (memProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent)) {
            memIdx = i; break;
        }
    }
    if (memIdx == UINT32_MAX) { LOGE("dumpFrame: no host-visible memory"); return; }
    vk::MemoryAllocateInfo mai(mreq.size, memIdx);
    auto ar = _device.allocateMemory(mai);
    if (ar.result != vk::Result::eSuccess) { LOGE("dumpFrame: allocateMemory failed"); return; }
    _dumpMemory = std::move(ar.value);
    vk::BindBufferMemoryInfo bbmi(*_dumpBuffer, *_dumpMemory, 0);
    _device.bindBufferMemory2({bbmi});

    vk::CommandPoolCreateInfo cpci{};
    cpci.flags = vk::CommandPoolCreateFlagBits::eTransient;
    cpci.queueFamilyIndex = _graphicsFamily;
    auto cpr = _device.createCommandPool(cpci);
    if (cpr.result != vk::Result::eSuccess) { LOGE("dumpFrame: createCommandPool failed"); return; }
    _dumpPool = std::move(cpr.value);
    vk::CommandBufferAllocateInfo cba(*_dumpPool, vk::CommandBufferLevel::ePrimary, 1);
    auto cbr = _device.allocateCommandBuffers(cba);
    if (cbr.result != vk::Result::eSuccess) { LOGE("dumpFrame: alloc cmd failed"); return; }
    _dumpCmd = std::move(cbr.value[0]);

    vk::Image img = _swapchain->image(idx);
    _dumpImages = {img};
    vk::CommandBufferBeginInfo cbbi(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    _dumpCmd.begin(cbbi);
    vk::ImageMemoryBarrier barrier{};
    barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    barrier.oldLayout = vk::ImageLayout::ePresentSrcKHR;
    barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = img;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    _dumpCmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                             vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, {barrier});
    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D(0, 0, 0);
    region.imageExtent = vk::Extent3D(ext.width, ext.height, 1);
    _dumpCmd.copyImageToBuffer(img, vk::ImageLayout::eTransferSrcOptimal, *_dumpBuffer, {region});
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
    _dumpCmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                             vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {}, {barrier});
    _dumpCmd.end();

    vk::FenceCreateInfo fci{};
    auto fr = _device.createFence(fci);
    if (fr.result != vk::Result::eSuccess) { LOGE("dumpFrame: createFence failed"); return; }
    _dumpFence = std::move(fr.value);

    vk::CommandBuffer cb = *_dumpCmd;
    vk::SubmitInfo si{};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    _graphicsQueue.submit({si}, *_dumpFence);
    (void)_device.waitForFences({*_dumpFence}, vk::True, UINT64_MAX);

    void* mapped = nullptr;
    auto mr = _dumpMemory.mapMemory(0, pixelBytes);
    if (mr.result != vk::Result::eSuccess) { LOGE("dumpFrame: mapMemory failed"); return; }
    mapped = mr.value;
    unsigned char* px = static_cast<unsigned char*>(mapped);
    std::string path = dumpPath;
    const bool isF16 = (_swapchain->format() == vk::Format::eR16G16B16A16Sfloat);
    const uint32_t bpp = isF16 ? 8u : 4u;
    {
        FILE* fp = std::fopen(path.c_str(), "wb");
        if (!fp) { LOGE("dumpFrame: cannot open {}", path); _dumpMemory.unmapMemory(); return; }
        std::fprintf(fp, "P6\n%u %u\n255\n", ext.width, ext.height);
        for (uint32_t y = 0; y < ext.height; y++) {
            const unsigned char* row = px + static_cast<size_t>(y) * ext.width * bpp;
            for (uint32_t x = 0; x < ext.width; x++) {
                float r, g, b;
                if (isF16) {
                    const uint16_t* p = reinterpret_cast<const uint16_t*>(row + static_cast<size_t>(x) * 8);
                    const auto half = [](uint16_t h) {
                        uint32_t s = (h >> 15) & 1, e = (h >> 10) & 0x1f, m = h & 0x3ff;
                        if (e == 0) return m == 0 ? 0.0f : (m / 1024.0f) * (1.0f / 16384.0f);
                        if (e == 31) return m == 0 ? (s ? -1e30f : 1e30f) : 0.0f;
                        int ex = static_cast<int>(e) - 15;
                        float v = (1.0f + m / 1024.0f) * std::pow(2.0f, static_cast<float>(ex));
                        return s ? -v : v;
                    };
                    r = half(p[2]); g = half(p[1]); b = half(p[0]);
                } else {
                    b = row[x * 4 + 0] / 255.0f; g = row[x * 4 + 1] / 255.0f; r = row[x * 4 + 2] / 255.0f;
                }
                auto cl = [](float v) { return static_cast<int>((v < 0 ? 0 : (v > 1 ? 1 : v)) * 255.0f + 0.5f); };
                std::fputc(cl(r), fp); std::fputc(cl(g), fp); std::fputc(cl(b), fp);
            }
        }
        std::fclose(fp);
    }
    _dumpMemory.unmapMemory();
    LOGI("dumpFrame: saved {} ({}x{})", path, ext.width, ext.height);

    uint32_t black = 0, nonblack = 0;
    for (uint64_t i = 0; i < ext.width * ext.height; i++) {
        const unsigned char* p = px + i * bpp;
        uint8_t c0, c1, c2;
        if (isF16) {
            const uint16_t* q = reinterpret_cast<const uint16_t*>(p);
            const auto half = [](uint16_t h) -> float {
                uint32_t e = (h >> 10) & 0x1f, m = h & 0x3ff;
                if (e == 0) return 0.0f;
                if (e == 31) return 1.0f;
                int ex = static_cast<int>(e) - 15;
                return (1.0f + m / 1024.0f) * std::pow(2.0f, static_cast<float>(ex));
            };
            float r = half(q[2]), g = half(q[1]), b2 = half(q[0]);
            c0 = static_cast<uint8_t>((r < 0 ? 0 : (r > 1 ? 1 : r)) * 255.0f);
            c1 = static_cast<uint8_t>((g < 0 ? 0 : (g > 1 ? 1 : g)) * 255.0f);
            c2 = static_cast<uint8_t>((b2 < 0 ? 0 : (b2 > 1 ? 1 : b2)) * 255.0f);
        } else {
            c0 = p[0]; c1 = p[1]; c2 = p[2];
        }
        if (c0 < 8 && c1 < 8 && c2 < 8) black++; else nonblack++;
    }
    LOGI("dumpFrame: pixels black={} nonblack={}", black, nonblack);
    _device.waitIdle();
}

void VKRenderer::onUniformCreated(VKBuffer* buffer, size_t offset, size_t size) {
    _uboBuffer = buffer;
    _uboSlotOffset = offset;
    _uboSlotSize = size;
    updateUboDescriptor();
}

void VKRenderer::onUniformUpdated(VKBuffer* buffer, uint32_t slot, size_t offset, size_t size) {
    if (_uboBuffer != buffer) return;
    _uboSlotIndex = slot;
    _uboSlotOffset = offset;
    _uboSlotSize = size;
    updateUboDescriptor();
}

void VKRenderer::updateUboDescriptor() {
    if (_uboDs.empty() || _uboBuffer == nullptr) return;
    DescriptorSet& ds = _uboDs[_uboSlotIndex % _uboDs.size()];
    if (ds == nullptr) return;
    vk::DescriptorBufferInfo info(_uboBuffer->raw(), _uboSlotOffset, _uboSlotSize);
    vk::WriteDescriptorSet wds{};
    wds.dstSet = *ds;
    wds.dstBinding = 0;
    wds.dstArrayElement = 0;
    wds.descriptorCount = 1;
    wds.descriptorType = vk::DescriptorType::eUniformBuffer;
    wds.pBufferInfo = &info;
    _device.updateDescriptorSets({wds}, {});
}

void VKRenderer::clearColor(float r, float g, float b, float a) {
    _clearColor[0] = r;
    _clearColor[1] = g;
    _clearColor[2] = b;
    _clearColor[3] = a;
}

void VKRenderer::setViewport(const Viewport& vp) {
    _viewport = vp;
    _viewportSet = true;
    if (_recording && _rpActive) applyViewport();
}

void VKRenderer::setPipeline(const std::shared_ptr<IPipeline>& pipeline) { _pipeline = pipeline; }
void VKRenderer::setVertexBuffer(const std::shared_ptr<IBuffer>& buffer) { _vertexBuffers[0] = buffer; }
void VKRenderer::setVertexBuffer(const std::shared_ptr<IBuffer>& buffer, uint32_t binding) {
    if (binding < _vertexBuffers.size()) _vertexBuffers[binding] = buffer;
}
void VKRenderer::setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) { _indexBuffer = buffer; }

void VKRenderer::setRenderTarget(const std::shared_ptr<IRenderTarget>& target) {
    if (_recording && _rpActive) {
        _cmd.endRenderPass();
        _rpActive = false;
    }
    _renderTarget = target;
    _vkRenderTarget = target ? std::dynamic_pointer_cast<VKRenderTarget>(target) : nullptr;
}

void VKRenderer::bindTexture(const std::shared_ptr<ITexture2D>& texture, unsigned int unit) {
    if (!texture || unit > 14) return;
    auto vktex = std::dynamic_pointer_cast<VKTexture2D>(texture);
    if (!vktex || !vktex->valid()) return;
    updateSamplerDescriptor(unit + 1, vktex->sampler(), vktex->view());
}

void VKRenderer::bindTexture(const std::shared_ptr<ITexture3D>& texture, unsigned int unit) {
    if (!texture || unit > 14) return;
    auto vktex = std::dynamic_pointer_cast<VKTexture3D>(texture);
    if (!vktex || !vktex->valid()) return;
    updateSamplerDescriptor(unit + 1, vktex->sampler(),
                            vktex->isDepth() ? vktex->depthCubeView() : vktex->cubeView());
}

void VKRenderer::bindTexture(rhi::ITexture2D* texture, unsigned int unit) {
    if (!texture || unit > 14) return;
    auto vktex = dynamic_cast<VKTexture2D*>(texture);
    if (!vktex || !vktex->valid()) return;
    updateSamplerDescriptor(unit + 1, vktex->sampler(), vktex->view());
}

void VKRenderer::updateSamplerDescriptor(uint32_t binding, vk::Sampler sampler, vk::ImageView view) {
    if (_uboDs.empty() || !sampler || !view) return;
    // sampler 写入所有 UBO slot 对应的 set：draw 绑定的 set 可能不同于最后一次
    // bindTexture 时的 set（slot 由每次 uniform update 推进），因此每个 set 都要
    // 持有同一 sampler，否则 llvmpipe 读到未绑的 sampler 会崩。
    vk::DescriptorImageInfo info(sampler, view, vk::ImageLayout::eShaderReadOnlyOptimal);
    for (auto& set : _uboDs) {
        if (!*set) continue;
        vk::WriteDescriptorSet wds{};
        wds.dstSet = *set;
        wds.dstBinding = binding;
        wds.dstArrayElement = 0;
        wds.descriptorCount = 1;
        wds.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        wds.pImageInfo = &info;
        _device.updateDescriptorSets({wds}, {});
    }
}

void VKRenderer::blitFramebuffer(const std::shared_ptr<IRenderTarget>& src,
                                 const std::shared_ptr<IRenderTarget>& dst, BlitMask mask) {
    if (!_recording || !src || !dst) return;
    auto s = std::dynamic_pointer_cast<VKRenderTarget>(src);
    auto d = std::dynamic_pointer_cast<VKRenderTarget>(dst);
    if (!s || !d || s->colorCount() == 0 || d->colorCount() == 0) return;
    if (_rpActive) {
        _cmd.endRenderPass();
        _rpActive = false;
    }

    const bool doColor = (static_cast<uint8_t>(mask) & static_cast<uint8_t>(BlitMask::Color)) != 0;
    const bool doDepth = (static_cast<uint8_t>(mask) & static_cast<uint8_t>(BlitMask::Depth)) != 0;

    if (doColor) {
        vk::Image srcImg = s->colorImage(0);
        vk::Image dstImg = d->colorImage(0);
        const vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor;

        // colorImage(0) returns the samplerable attachment (the resolve image
        // for MSAA, the color image otherwise), whose layout after the render
        // pass is eShaderReadOnlyOptimal.
        vk::ImageMemoryBarrier toSrc{};
        toSrc.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        toSrc.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        toSrc.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
        toSrc.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
        toSrc.image = srcImg;
        toSrc.subresourceRange.aspectMask = aspect;
        toSrc.subresourceRange.levelCount = 1;
        toSrc.subresourceRange.layerCount = 1;
        toSrc.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        toSrc.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        vk::ImageMemoryBarrier toDst{};
        toDst.oldLayout = vk::ImageLayout::eUndefined;
        toDst.newLayout = vk::ImageLayout::eTransferDstOptimal;
        toDst.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
        toDst.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
        toDst.image = dstImg;
        toDst.subresourceRange.aspectMask = aspect;
        toDst.subresourceRange.levelCount = 1;
        toDst.subresourceRange.layerCount = 1;
        toDst.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        _cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer,
                             {}, {}, {}, {toSrc, toDst});

        vk::ImageBlit region{};
        region.srcSubresource.aspectMask = aspect;
        region.srcSubresource.layerCount = 1;
        region.srcOffsets[1] = vk::Offset3D(static_cast<int32_t>(s->extent2d().width),
                                            static_cast<int32_t>(s->extent2d().height), 1);
        region.dstSubresource.aspectMask = aspect;
        region.dstSubresource.layerCount = 1;
        region.dstOffsets[1] = vk::Offset3D(static_cast<int32_t>(d->extent2d().width),
                                            static_cast<int32_t>(d->extent2d().height), 1);
        _cmd.blitImage(srcImg, vk::ImageLayout::eTransferSrcOptimal,
                       dstImg, vk::ImageLayout::eTransferDstOptimal, {region}, vk::Filter::eLinear);

        vk::ImageMemoryBarrier backSrc{};
        backSrc.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        backSrc.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        backSrc.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
        backSrc.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
        backSrc.image = srcImg;
        backSrc.subresourceRange.aspectMask = aspect;
        backSrc.subresourceRange.levelCount = 1;
        backSrc.subresourceRange.layerCount = 1;
        backSrc.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        backSrc.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        vk::ImageMemoryBarrier backDst{};
        backDst.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        backDst.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        backDst.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
        backDst.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
        backDst.image = dstImg;
        backDst.subresourceRange.aspectMask = aspect;
        backDst.subresourceRange.levelCount = 1;
        backDst.subresourceRange.layerCount = 1;
        backDst.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        backDst.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        _cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                             {}, {}, {}, {backSrc, backDst});
    }
    (void)doDepth;
}

void VKRenderer::draw(uint32_t vertexCount, uint32_t firstVertex) {
    if (!_recording || !_pipeline) return;
    if (!ensureRenderPass()) return;
    if (!bindPipelineAndState()) return;
    bindVertexBuffers();
    _cmd.draw(vertexCount, 1, firstVertex, 0);
}

void VKRenderer::drawIndexed(uint32_t indexCount, uint32_t indexOffset, uint32_t vertexOffset) {
    if (!_recording || !_pipeline) return;
    if (!ensureRenderPass()) return;
    if (!bindPipelineAndState()) return;
    auto ib = std::dynamic_pointer_cast<VKBuffer>(_indexBuffer);
    if (!ib || !ib->raw()) return;
    bindVertexBuffers();
    _cmd.bindIndexBuffer(ib->raw(), 0, vk::IndexType::eUint32);
    _cmd.drawIndexed(indexCount, 1, indexOffset, static_cast<int32_t>(vertexOffset), 0);
}

void VKRenderer::drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t indexOffset, uint32_t vertexOffset) {
    if (!_recording || !_pipeline) return;
    if (!ensureRenderPass()) return;
    if (!bindPipelineAndState()) return;
    auto ib = std::dynamic_pointer_cast<VKBuffer>(_indexBuffer);
    if (!ib || !ib->raw()) return;
    bindVertexBuffers();
    _cmd.bindIndexBuffer(ib->raw(), 0, vk::IndexType::eUint32);
    _cmd.drawIndexed(indexCount, instanceCount, indexOffset, static_cast<int32_t>(vertexOffset), 0);
}

void VKRenderer::drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) {
    if (!_recording || !_pipeline) return;
    if (!ensureRenderPass()) return;
    if (!bindPipelineAndState()) return;
    bindVertexBuffers();
    _cmd.draw(vertexCount, instanceCount, firstVertex, 0);
}

bool VKRenderer::ensureRenderPass() {
    if (_rpActive) return true;
    std::shared_ptr<VKRenderTarget> vkrt = _vkRenderTarget;
    vk::RenderPass rp;
    vk::Framebuffer fb;
    vk::Extent2D ext;
    uint32_t colorCount = 1;
    if (vkrt && vkrt->valid()) {
        rp = vkrt->renderPass();
        fb = vkrt->framebuffer();
        ext = vkrt->extent2d();
        colorCount = vkrt->colorCount();
    } else {
        const uint32_t idx = _swapchain->currentImage();
        if (idx >= _framebuffers.size()) return false;
        rp = *_renderPass;
        fb = *_framebuffers[idx];
        ext = extent();
    }

    // Build clear values in the render pass's attachment order:
    // [color] x N, [resolve] x N (when MSAA), [depth] (optional). The resolve
    // attachments use loadOp DontCare so their clear value is ignored, but a
    // VkClearValue must still be supplied for every attachment.
    const bool msaa = vkrt && vkrt->valid() && vkrt->msaa();
    const bool hasDepth = vkrt && vkrt->valid() && vkrt->hasDepthAttachment();
    std::vector<vk::ClearValue> clears;
    for (uint32_t i = 0; i < colorCount; i++) {
        vk::ClearValue cv;
        cv.color = vk::ClearColorValue(_clearColor[0], _clearColor[1], _clearColor[2], _clearColor[3]);
        clears.push_back(cv);
    }
    if (msaa) {
        for (uint32_t i = 0; i < colorCount; i++) {
            vk::ClearValue cv;
            cv.color = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f);
            clears.push_back(cv);
        }
    }
    if (hasDepth) {
        vk::ClearValue cv;
        cv.depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
        clears.push_back(cv);
    }

    const vk::Rect2D area({0, 0}, ext);
    vk::RenderPassBeginInfo rpbi(rp, fb, area, static_cast<uint32_t>(clears.size()), clears.data());
    _cmd.beginRenderPass(rpbi, vk::SubpassContents::eInline);
    _rpActive = true;
    applyViewport();
    return true;
}

void VKRenderer::applyViewport() {
    const vk::Extent2D ext = extent();
    float x = _viewportSet ? static_cast<float>(_viewport.x) : 0.0f;
    float y = _viewportSet ? static_cast<float>(_viewport.y) : 0.0f;
    float w = _viewportSet ? static_cast<float>(_viewport.width) : static_cast<float>(ext.width);
    float h = _viewportSet ? static_cast<float>(_viewport.height) : static_cast<float>(ext.height);
    // VK NDC y 向下，App 投影矩阵为 GL 语义（y 向上）：用负高度 viewport 翻转 y 轴。
    // 深度：GL 投影 z 属 [-1,1]，负高度 viewport 的深度变换 z_ndc/2+0.5 恰好映射到 VK [0,1]。
    vk::Viewport vp(x, y + h, w, -h, 0.0f, 1.0f);
    vk::Rect2D sc({static_cast<int32_t>(x), static_cast<int32_t>(y)},
                  {static_cast<uint32_t>(w), static_cast<uint32_t>(h)});
    _cmd.setViewport(0, {vp});
    _cmd.setScissor(0, {sc});
}

bool VKRenderer::bindPipelineAndState() {
    auto vkp = std::dynamic_pointer_cast<VKPipeline>(_pipeline);
    if (!vkp) return false;
    // Build/bind a pipeline matching the currently active render pass (either an
    // offscreen RT or the swapchain), with the correct attachment sample count.
    vk::RenderPass rp;
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
    if (_vkRenderTarget && _vkRenderTarget->valid()) {
        rp = _vkRenderTarget->renderPass();
        samples = _vkRenderTarget->msaa()
            ? ToVkSamples(static_cast<int>(_vkRenderTarget->samples()))
            : vk::SampleCountFlagBits::e1;
    } else {
        rp = *_renderPass;
    }
    vk::Pipeline p = vkp->pipelineFor(rp, samples);
    if (!p) return false;
    _cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, p);
    // bind描述符集：每个 draw 绑到当前 UBO slot 对应的 set
    if (!_uboDs.empty()) {
        DescriptorSet& ds = _uboDs[_uboSlotIndex % _uboDs.size()];
        if (*ds) _cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_pipelineLayout, 0, {*ds}, {});
    }
    vkp->applyDynamicState(_cmd);
    return true;
}

bool VKRenderer::bindVertexBuffers() {
    // Vulkan binds a contiguous range [firstBinding, firstBinding+count) in one
    // call, and binding a null VkBuffer for an empty slot is illegal. An App's
    // VertexLayout may leave gaps (e.g. only bindings 0 and 2 present, binding 1
    // unused), so emit one vkCmdBindVertexBuffers per contiguous run of present
    // bindings, each starting at its real binding index.
    uint32_t first = UINT32_MAX;
    std::vector<vk::Buffer> bufs;
    std::vector<vk::DeviceSize> offs;
    for (uint32_t i = 0; i < _vertexBuffers.size(); i++) {
        if (!_vertexBuffers[i]) continue;
        auto vb = std::dynamic_pointer_cast<VKBuffer>(_vertexBuffers[i]);
        if (!vb) continue;
        if (first == UINT32_MAX || i != first + bufs.size()) {
            if (first != UINT32_MAX) _cmd.bindVertexBuffers(first, bufs, offs);
            first = i;
            bufs.clear();
            offs.clear();
        }
        bufs.push_back(vb->raw());
        offs.push_back(0);
    }
    if (first != UINT32_MAX) _cmd.bindVertexBuffers(first, bufs, offs);
    return true;
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

BackendCapabilities VKRenderer::backendCapabilities() {
    BackendCapabilities caps;
    caps.maxSamples = 8;
    caps.maxUniformBlockSize = 65536;
    return caps;
}

bool VKRenderer::imguiInitInfo(VKImGuiInitInfo& out) {
    if (_device == nullptr || !_swapchain) return false;
    out.instance = *_instance;
    out.physDevice = *_phys;
    out.device = *_device;
    out.graphicsFamily = _graphicsFamily;
    out.graphicsQueue = *_graphicsQueue;
    out.dsPool = *_dsPool;
    out.imageCount = _swapchain->imageCount();
    out.renderPass = *_renderPass;
    return true;
}

void VKRenderer::renderImGuiDrawData(void* drawData) {
    if (!drawData || !_recording || !_rpActive) return;
    ImGui_ImplVulkan_RenderDrawData(static_cast<ImDrawData*>(drawData),
                                    static_cast<VkCommandBuffer>(*_cmd));
    // ImGui backend 渲染时会把 viewport/scissor 设为自身的正高度值。bindPipelineAndState
    // → VKPipeline::applyDynamicState 不会重设 viewport/scissor（它们仅在 ensureRenderPass
    // 与 setViewport 处应用），因此这里主动重放本后端的负高度 viewport，保证同一 render
    // pass 内后续 3D draw 仍为正确的上下翻转。
    applyViewport();
}

std::shared_ptr<IRenderer> createVKRenderer() {
    return std::make_shared<VKRenderer>();
}

} // namespace rhi