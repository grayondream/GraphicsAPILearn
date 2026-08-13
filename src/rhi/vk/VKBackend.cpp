#include "VKBackend.hpp"
#include "VKHeader.hpp"
#include "VKSwapchain.hpp"
#include "VKBuffer.hpp"
#include "VKShader.hpp"
#include "VKPipeline.hpp"
#include "VKTexture2D.hpp"
#include "VKTexture3D.hpp"
#include <GLFW/glfw3.h>
#include "rhi/core/ISurface.hpp"
#include "base/Log.hpp"
#include <cstring>
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
    void setRenderTarget(const std::shared_ptr<IRenderTarget>&) override {}
    void bindTexture(const std::shared_ptr<ITexture2D>&, unsigned int) override {}
    void bindTexture(const std::shared_ptr<ITexture3D>&, unsigned int) override {}
    void bindTexture(rhi::ITexture2D*, unsigned int) override {}
    void draw(uint32_t vertexCount, uint32_t firstVertex) override;
    void drawIndexed(uint32_t indexCount, uint32_t indexOffset, uint32_t vertexOffset) override;
    void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t indexOffset, uint32_t vertexOffset) override;
    void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) override;
    void blitFramebuffer(const std::shared_ptr<IRenderTarget>&, const std::shared_ptr<IRenderTarget>&, BlitMask) override {}
    BackendCapabilities backendCapabilities() override;

    void onUniformCreated(VKBuffer* buffer) override;
    void onUniformUpdated(VKBuffer* buffer) override;

private:
    bool pickPhysicalDevice();
    bool isDeviceSuitable(vk::raii::PhysicalDevice& pd);
    QueueFamilies findQueueFamilies(vk::raii::PhysicalDevice& pd);
    bool createDevice(const QueueFamilies& families);
    bool createDescriptors();
    bool createRenderPassAndFramebuffers();
    bool createCommandResources();
    void updateUboDescriptor();
    bool ensureRenderPass();
    void applyViewport();
    bool bindPipelineAndState();
    bool bindVertexBuffers();
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
    vk::raii::DescriptorSet _uboDs{nullptr};
    vk::raii::PipelineLayout _pipelineLayout{nullptr};
    vk::raii::RenderPass _renderPass{nullptr};
    std::vector<vk::raii::Framebuffer> _framebuffers{};
    vk::raii::CommandPool _cmdPool{nullptr};
    vk::raii::CommandBuffer _cmd{nullptr};
    vk::raii::Semaphore _imageReady{nullptr};
    vk::raii::Semaphore _rendered{nullptr};
    vk::raii::Fence _frameFence{nullptr};

    bool _recording{false};
    bool _rpActive{false};
    bool _viewportSet{false};
    Viewport _viewport{};
    float _clearColor[4]{0.0f, 0.0f, 0.0f, 1.0f};
    std::shared_ptr<IPipeline> _pipeline{};
    std::array<std::shared_ptr<IBuffer>, 16> _vertexBuffers{};
    std::shared_ptr<IBuffer> _indexBuffer{};
    VKBuffer* _uboBuffer{nullptr};
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
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 16),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 16 * 15),
    };
    vk::DescriptorPoolCreateInfo dpci{};
    dpci.maxSets = 16;
    dpci.poolSizeCount = 2;
    dpci.pPoolSizes = sizes;
    auto dpr = _device.createDescriptorPool(dpci);
    if (dpr.result != vk::Result::eSuccess) {
        LOGE("VKRenderer: createDescriptorPool failed");
        return false;
    }
    _dsPool = std::move(dpr.value);

    vk::DescriptorSetAllocateInfo dsai(*_dsPool, 1, &*_dsLayout);
    auto dar = _device.allocateDescriptorSets(dsai);
    if (dar.result != vk::Result::eSuccess) {
        LOGE("VKRenderer: allocateDescriptorSets failed");
        return false;
    }
    _uboDs = std::move(dar.value[0]);

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
    _uboDs = vk::raii::DescriptorSet{nullptr};
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
    return std::make_shared<VKPipeline>(_device, *_pipelineLayout, *_renderPass,
                                        _swapchain->format(), layout, vks);
}

std::shared_ptr<IBuffer> VKRenderer::createBuffer() {
    return std::make_shared<VKBuffer>(_device, _phys, _graphicsQueue, _graphicsFamily);
}

std::shared_ptr<IBuffer> VKRenderer::createUniformBuffer() {
    auto buf = std::make_shared<VKBuffer>(_device, _phys, _graphicsQueue, _graphicsFamily);
    buf->setNotifier(this);
    return buf;
}

std::shared_ptr<ITexture2D> VKRenderer::createTexture2D() { return std::make_shared<VKTexture2D>(_device); }
std::shared_ptr<ITexture3D> VKRenderer::createTexture3D() { return std::make_shared<VKTexture3D>(_device); }
std::shared_ptr<IRenderTarget> VKRenderer::createRenderTarget() { return {}; }
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
    _cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_pipelineLayout, 0, {*_uboDs}, {});
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
    _swapchain->setPresentSemaphore(static_cast<vk::Semaphore>(*_rendered));
    return _swapchain->present();
}

void VKRenderer::onUniformCreated(VKBuffer* buffer) {
    _uboBuffer = buffer;
    updateUboDescriptor();
}

void VKRenderer::onUniformUpdated(VKBuffer* buffer) {
    if (_uboBuffer == buffer) updateUboDescriptor();
}

void VKRenderer::updateUboDescriptor() {
    if (_uboDs == nullptr || _uboBuffer == nullptr) return;
    vk::DescriptorBufferInfo info(_uboBuffer->raw(), 0, _uboBuffer->size());
    vk::WriteDescriptorSet wds{};
    wds.dstSet = *_uboDs;
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
    const uint32_t idx = _swapchain->currentImage();
    if (idx >= _framebuffers.size()) return false;
    vk::ClearValue clear;
    clear.color = vk::ClearColorValue(_clearColor[0], _clearColor[1], _clearColor[2], _clearColor[3]);
    const vk::Rect2D area({0, 0}, extent());
    vk::RenderPassBeginInfo rpbi(*_renderPass, *_framebuffers[idx], area, 1, &clear);
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
    vk::Viewport vp(x, y, w, h, 0.0f, 1.0f);
    vk::Rect2D sc({static_cast<int32_t>(x), static_cast<int32_t>(y)},
                  {static_cast<uint32_t>(w), static_cast<uint32_t>(h)});
    _cmd.setViewport(0, {vp});
    _cmd.setScissor(0, {sc});
}

bool VKRenderer::bindPipelineAndState() {
    auto vkp = std::dynamic_pointer_cast<VKPipeline>(_pipeline);
    if (!vkp) return false;
    if (!vkp->ensureCreated()) return false;
    _cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, vkp->pipeline());
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
    caps.maxSamples = 1;
    caps.maxUniformBlockSize = 16384;
    return caps;
}

std::shared_ptr<IRenderer> createVKRenderer() {
    return std::make_shared<VKRenderer>();
}

} // namespace rhi