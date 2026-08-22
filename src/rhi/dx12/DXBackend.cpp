#include "rhi/dx12/DXBackend.hpp"
#include "rhi/dx12/DXBuffer.hpp"
#include "rhi/dx12/DXPipeline.hpp"
#include "rhi/dx12/DXShader.hpp"
#include "rhi/dx12/DXSwapchain.hpp"
#include "rhi/core/ISurface.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/ITexture3D.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ISwapchain.hpp"

#include <array>
#include <map>

namespace rhi {

namespace {

// 纹理/离屏 RT 工厂暂返回 no-op 对象而非 nullptr：其 create()==false 使样例在
// 加载期经 ExitIfFailed 干净退出，避免空指针解引用段错误。Task 7/8 以同名真实类替换。

class DXNullPipeline : public IPipeline {
public:
    void use() override {}
    void* handle() override { return nullptr; }
    bool setUniform(const std::string&, bool) override { return false; }
    bool setUniform(const std::string&, int) override { return false; }
    bool setUniform(const std::string&, float) override { return false; }
    bool setUniform(const std::string&, const float*, int) override { return false; }
    bool setUniform(const std::string&, const float*, int, int) override { return false; }
    bool setUniformMatrix(const std::string&, const float*, int, int) override { return false; }
    void bindUniformBlock(uint32_t) override {}
    void setDepthTest(bool) override {}
    void setCullMode(bool, int) override {}
    void setBlend(bool) override {}
    void setDepthFunc(CompareFunc) override {}
    void setDepthMask(bool) override {}
    void setStencilTest(bool) override {}
    void setStencilFunc(CompareFunc, int, unsigned) override {}
    void setStencilOp(StencilOp, StencilOp, StencilOp) override {}
    void setStencilMask(unsigned) override {}
    void setBlendFunc(BlendFactor, BlendFactor) override {}
    void setCullFaceEnable(bool) override {}
    void setCullFace(CullFace) override {}
    void setFrontFace(bool) override {}
    void setPolygonMode(PolygonMode) override {}
    void setPointSizeProgramEnable(bool) override {}
    void setMultisample(bool) override {}
    void setPrimitiveType(PrimitiveType type) override { _primitive = type; }
    PrimitiveType primitiveType() const override { return _primitive; }
    void setVertexBuffer(const std::shared_ptr<IBuffer>&, uint32_t) override {}
    void setIndexBuffer(const std::shared_ptr<IBuffer>&) override {}

private:
    PrimitiveType _primitive{PrimitiveType::TriangleList};
};

class DXNullBuffer : public IBuffer {
public:
    bool init(const void*, size_t, BufferType) override { return false; }
    bool update(const void*, size_t, size_t) override { return false; }
    bool bindRange(uint32_t, size_t, size_t) override { return false; }
    bool bind() override { return false; }
    void* handle() override { return nullptr; }
};

class DXNullTexture2D : public ITexture2D {
public:
    bool init(const TextureDataView2D&) override { return false; }
    bool init(const TextureDesc&, const TextureDataView2D&) override { return false; }
    bool createEmpty(const TextureDesc&, int, int) override { return false; }
    void bind(unsigned int) override {}
    void* handle() override { return nullptr; }
    bool valid() const override { return false; }
    void release() override {}
};

class DXNullTexture3D : public ITexture3D {
public:
    bool init(const TextureDataView3D&) override { return false; }
    bool initCube(const TextureDesc&, const TextureDataView2D*) override { return false; }
    bool createEmpty(const TextureDesc&, int, int) override { return false; }
    void bind(unsigned int) override {}
    void* handle() override { return nullptr; }
    bool valid() const override { return false; }
    void release() override {}
};

class DXNullRenderTarget : public IRenderTarget {
public:
    bool create(int, int) override { return false; }
    bool create(const FramebufferDesc&) override { return false; }
    bool attachCubeFace(ITexture3D*, int, int) override { return false; }
    bool attachDepthCube(ITexture3D*, int) override { return false; }
    bool bind() override { return false; }
    bool unbind() override { return false; }
    void* colorTexture() override { return nullptr; }
    ITexture2D* colorTexture2D(int) override { return nullptr; }
    ITexture2D* depthTexture2D() override { return nullptr; }
    bool resolveTo(IRenderTarget&) override { return false; }
    void* handle() override { return nullptr; }
    void release() override {}
};

// 只警告一次的桩实现提示（纹理/blit 属 Task 7/8 范围）
void WarnOnce(const char* message) {
    struct Flag { bool done{false}; };
    static std::map<const char*, Flag> warned;
    auto& f = warned[message];
    if (!f.done) {
        f.done = true;
        LOGW("[DX12] {}", message);
    }
}

D3D12_PRIMITIVE_TOPOLOGY ToDxTopology(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::TriangleList:  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveType::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case PrimitiveType::Lines:         return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveType::Points:        return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    }
    return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

} // namespace

class DXRenderer : public IRenderer {
public:
    ~DXRenderer() override { shutdown(); }

    bool init(const std::shared_ptr<ISurface>& surface) override;
    void shutdown() override;

    // ---- 资源工厂 ----
    std::shared_ptr<IShader> createShader() override { return std::make_shared<DXShader>(); }
    std::shared_ptr<IPipeline> createPipeline(const VertexLayout& layout, const std::shared_ptr<IShader>& shader) override {
        auto dxShader = std::dynamic_pointer_cast<DXShader>(shader);
        if (!_device.ptr || !_rootSignature.ptr || !dxShader || !dxShader->valid()) {
            LOGE("[DX12] createPipeline: device/root signature/shader not ready, null fallback");
            return std::make_shared<DXNullPipeline>();
        }
        return std::make_shared<DXPipeline>(_device.ptr, _rootSignature.ptr, layout, dxShader);
    }
    std::shared_ptr<IBuffer> createBuffer() override {
        if (!_device.ptr) { LOGE("[DX12] createBuffer before init"); return std::make_shared<DXNullBuffer>(); }
        return std::make_shared<DXBuffer>(_device.ptr, _queue.ptr, _uploadAllocator.ptr, _frameFence.ptr, _fenceEvent);
    }
    std::shared_ptr<IBuffer> createUniformBuffer() override {
        if (!_device.ptr) { LOGE("[DX12] createUniformBuffer before init"); return std::make_shared<DXNullBuffer>(); }
        auto buf = std::make_shared<DXBuffer>(_device.ptr, _queue.ptr, _uploadAllocator.ptr, _frameFence.ptr, _fenceEvent);
        // 每个样例持且仅持一个 UniformBlock 缓冲：最近创建者即当前绑定目标，
        // draw 时读其 submittedBase() 换算 ring 槽基址直挂根 CBV b0
        _uniformBuffer = buf;
        return buf;
    }
    std::shared_ptr<ITexture2D> createTexture2D() override { return std::make_shared<DXNullTexture2D>(); }
    std::shared_ptr<ITexture3D> createTexture3D() override { return std::make_shared<DXNullTexture3D>(); }
    std::shared_ptr<IRenderTarget> createRenderTarget() override { return std::make_shared<DXNullRenderTarget>(); }
    std::shared_ptr<ISwapchain> getSwapchain() override { return _swapchain; }

    // ---- 帧控制 ----
    void beginFrame() override {
        if (!_swapchain || !_swapchain->initialized()) return;
        ensureRecording();
        setRenderTarget(nullptr);
    }
    void endFrame() override {
        // VK 曾漏清标志致帧间隙热切换后 RP 状态错乱：此处显式归零
        _rtActive = false;
    }
    bool present() override;

    // ---- 状态与绘制 ----
    void clearColor(float r, float g, float b, float a) override {
        _clearColor[0] = r; _clearColor[1] = g; _clearColor[2] = b; _clearColor[3] = a;
        void* key = _renderTarget ? static_cast<void*>(_renderTarget.get()) : nullptr;
        _clearColors[key] = {r, g, b, a};
        // GL 语义：清"调用时绑定"的目标。窗口 RT 已打开且无 pending 切换时直接清
        // （含深度/模板，同 GLBackend 的 COLOR|DEPTH|STENCIL 全清）
        if (_recording && !_omPending && _rtActive && !key) activateWindowTargets(true);
    }
    void setViewport(const Viewport& vp) override {
        _viewport = vp;
        _viewportSet = true;
        if (_recording && _rtActive) applyViewport();
    }
    void setPipeline(const std::shared_ptr<IPipeline>& pipeline) override {
        // p=null 时先落地 pending RT 变更；PSO 绑定延后到首个 draw——
        // PSOKey 依赖当时生效的 RT 格式/采样数（同 VK pipelineFor(rp, samples)）
        if (!pipeline && _omPending) flushOmTargets();
        _pipeline = pipeline;
    }
    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer) override { setVertexBuffer(buffer, 0); }
    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer, uint32_t binding) override {
        if (binding < _vertexBuffers.size()) _vertexBuffers[binding] = buffer;
    }
    void setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) override { _indexBuffer = buffer; }
    void setRenderTarget(const std::shared_ptr<IRenderTarget>& target) override {
        // VK 每次 RP 重开均清屏（loadOp=Clear），样例按该语义编写：
        // 重复切到同一目标同样触发重绑+清屏，保持三后端行为一致
        _renderTarget = target;
        _rtActive = false;
        _omPending = true;
    }
    void bindTexture(const std::shared_ptr<ITexture2D>&, unsigned int) override {
        WarnOnce("bindTexture(ITexture2D) is a stub until Task 7 (SRV heap)");
    }
    void bindTexture(const std::shared_ptr<ITexture3D>&, unsigned int) override {
        WarnOnce("bindTexture(ITexture3D) is a stub until Task 8 (cubemap)");
    }
    void bindTexture(rhi::ITexture2D*, unsigned int) override {
        WarnOnce("bindTexture(raw ITexture2D*) is a stub until Task 7 (SRV heap)");
    }
    void draw(uint32_t vertexCount, uint32_t firstVertex) override {
        if (!prepareDraw(false)) return;
        _cmdList.ptr->DrawInstanced(vertexCount, 1, firstVertex, 0);
    }
    void drawIndexed(uint32_t indexCount, uint32_t indexOffset, uint32_t vertexOffset) override {
        if (!prepareDraw(true)) return;
        _cmdList.ptr->DrawIndexedInstanced(indexCount, 1, indexOffset, vertexOffset, 0);
    }
    void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount,
                              uint32_t indexOffset, uint32_t vertexOffset) override {
        if (!prepareDraw(true)) return;
        _cmdList.ptr->DrawIndexedInstanced(indexCount, instanceCount, indexOffset, vertexOffset, 0);
    }
    void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) override {
        if (!prepareDraw(false)) return;
        _cmdList.ptr->DrawInstanced(vertexCount, instanceCount, firstVertex, 0);
    }
    void blitFramebuffer(const std::shared_ptr<IRenderTarget>&,
                         const std::shared_ptr<IRenderTarget>&, BlitMask) override {
        WarnOnce("blitFramebuffer is a stub until Task 7/8 (blit PSO / MSAA resolve)");
    }
    BackendCapabilities backendCapabilities() override {
        BackendCapabilities caps{};
        caps.maxUniformBlockSize = 64 * 1024;
        if (_device.Get()) {
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS q{};
            q.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            for (UINT n : {UINT{8}, UINT{4}}) {
                q.SampleCount = n;
                q.NumQualityLevels = 0;
                if (SUCCEEDED(_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
                                                           &q, sizeof(q))) &&
                    q.NumQualityLevels > 0) {
                    caps.maxSamples = static_cast<int>(n);
                    break;
                }
            }
        }
        return caps;
    }

    // ---- 样例切换 / 同步 ----
    void resetRenderState() override {
        // reloadSample 在 waitIdle 之后调用：清掉上一样例的资源引用，防止新样例
        // 首帧误用悬垂指针（对齐 VKRenderer 行为）
        _pipeline = nullptr;
        _indexBuffer = nullptr;
        _vertexBuffers = {};
        _uniformBuffer = nullptr;
        _renderTarget = nullptr;
        _clearColors.clear();
        _clearColor[0] = 0.0f; _clearColor[1] = 0.0f; _clearColor[2] = 0.0f; _clearColor[3] = 1.0f;
        _rtActive = false;
        _omPending = false;
        _backBufferBound = false;
        _viewportSet = false;
        _viewport = {};
    }
    void waitIdle() override { waitForGpuIdle(); }
    void flush() override {
        // 仅用于帧循环外的离屏预计算提交（IBL renderBeforeLoop 等，Task 8 生效）：
        // 关闭录制、提交并等待完成；不重新开录——后续绘制由 ensureRecording 懒启动
        if (!_recording || !_cmdList.ptr || !_queue.Get()) return;
        if (_omPending) flushOmTargets();
        DX_CHECK(_cmdList->Close(), "close command list (flush)");
        _recording = false;
        ID3D12CommandList* lists[] = {_cmdList.ptr};
        _queue->ExecuteCommandLists(1, lists);
        waitForGpuIdle();
    }

    // ImGui overlay 初始化信息（非虚接口，Task 9 由 ImGuiDirectx12Window 下转型调用；
    // srvHeap 随 Task 7 SRV 堆创建后回填）。using 防止该重载遮蔽基类同名虚函数
    using IRenderer::imguiInitInfo;
    bool imguiInitInfo(DXImGuiInitInfo& out) {
        out.device = _device.ptr;
        out.queue = _queue.ptr;
        out.srvHeap = nullptr;
        return _device.ptr && _queue.ptr;
    }

private:
    // 开录：Reset allocator+list。present 尾部已开录时幂等跳过；首帧或
    // flush() 之后由本函数懒启动（同 VK ensureRecording 模式）
    void ensureRecording() {
        if (_recording || !_device.ptr || !_frameAllocator.ptr || !_cmdList.ptr) return;
        DX_CHECK(_frameAllocator->Reset(), "reset frame allocator");
        DX_CHECK(_cmdList->Reset(_frameAllocator.ptr, nullptr), "reset frame command list");
        _recording = true;
        _rtActive = false;
    }

    // pending RT 变更落地：null=窗口路径（屏障+OMSetRenderTargets+Clear）；
    // 离屏 RT 为 Task 8 范围，命中时警告一次并回落窗口目标
    void flushOmTargets() {
        if (!_omPending || !_recording || !_cmdList.ptr) return;
        _omPending = false;
        if (_renderTarget) {
            WarnOnce("offscreen render targets land in Task 8; routing to window for now");
            _renderTarget = nullptr;
        }
        activateWindowTargets(true);
    }

    // 窗口目标激活：PRESENT→RENDER_TARGET 屏障（每帧一次，flip 模型要求呈现前
    // 回 PRESENT 态）→ OMSetRenderTargets(color+depth) → 可选清屏 → viewport
    void activateWindowTargets(bool doClear) {
        if (!_swapchain || !_swapchain->initialized() || !_cmdList.ptr) return;
        const uint32_t idx = _swapchain->currentIndex();
        ID3D12Resource* bb = _swapchain->backBuffer(idx);
        if (!bb) return;
        if (!_backBufferBound) {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = bb;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            _cmdList.ptr->ResourceBarrier(1, &barrier);
            _backBufferBound = true;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _swapchain->rtv(idx);
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = _swapchain->dsv();
        _cmdList.ptr->OMSetRenderTargets(1, &rtvHandle, FALSE, dsvHandle.ptr ? &dsvHandle : nullptr);
        if (doClear) {
            const std::array<float, 4> cc = clearColorFor(nullptr);
            _cmdList.ptr->ClearRenderTargetView(rtvHandle, cc.data(), 0, nullptr);
            if (dsvHandle.ptr) {
                _cmdList.ptr->ClearDepthStencilView(dsvHandle,
                                                    D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                                                    1.0f, 0, 0, nullptr);
            }
        }
        applyViewport();
        _rtActive = true;
    }

    std::array<float, 4> clearColorFor(void* key) const {
        auto it = _clearColors.find(key);
        return it != _clearColors.end() ? it->second
                                        : std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f};
    }

    // GL 投影矩阵 y-up 而 D3D12 NDC y-down：负高度 viewport 翻转显示（同 VK 方案），
    // MinDepth/MaxDepth=[0,1] 顺带把 GL z∈[-1,1] 线性映射进 D3D12 深度域
    void applyViewport() {
        if (!_cmdList.ptr || !_swapchain) return;
        const float x = _viewportSet ? static_cast<float>(_viewport.x) : 0.0f;
        const float y = _viewportSet ? static_cast<float>(_viewport.y) : 0.0f;
        const float w = (_viewportSet && _viewport.width > 0)
                            ? static_cast<float>(_viewport.width)
                            : static_cast<float>(_swapchain->width());
        const float h = (_viewportSet && _viewport.height > 0)
                            ? static_cast<float>(_viewport.height)
                            : static_cast<float>(_swapchain->height());
        D3D12_VIEWPORT vp{x, y + h, w, -h, 0.0f, 1.0f};
        D3D12_RECT scissor{static_cast<LONG>(x), static_cast<LONG>(y),
                           static_cast<LONG>(x + w), static_cast<LONG>(y + h)};
        _cmdList.ptr->RSSetViewports(1, &vp);
        _cmdList.ptr->RSSetScissorRects(1, &scissor);
    }

    // 公共绘制前奏：pending RT 落地 → PSO 取用/根签名 → UBO 根 CBV → 拓扑 → VB/IB
    bool prepareDraw(bool needsIndex);

    // 共享 fence 单调等待（Signal 必须 GetCompletedValue()+1 推进，同 DXBuffer upload）
    void waitForGpuIdle() {
        if (!_queue.Get() || !_frameFence.Get() || !_fenceEvent) return;
        const UINT64 value = _frameFence->GetCompletedValue() + 1;
        _queue->Signal(_frameFence.ptr, value);
        if (_frameFence->GetCompletedValue() < value) {
            _frameFence->SetEventOnCompletion(value, _fenceEvent);
            WaitForSingleObject(_fenceEvent, INFINITE);
        }
    }

    std::shared_ptr<ISurface> _surface;
    Viewport _viewport{};
    ComPtr<ID3D12Device> _device;
    ComPtr<ID3D12CommandQueue> _queue;
    ComPtr<ID3D12Fence> _frameFence;
    HANDLE _fenceEvent{nullptr};
    // 缓冲初始化数据的一次性拷贝命令与 VB/IB 上传共用此 direct allocator（串行使用，用后 Reset）
    ComPtr<ID3D12CommandAllocator> _uploadAllocator;
    // 全局单例 root signature（所有 DXPipeline 共享），生命周期与 device 一致
    ComPtr<ID3D12RootSignature> _rootSignature;
    // 帧录制资源：单 allocator+command list，帧间经 fence 全串行化
    ComPtr<ID3D12CommandAllocator> _frameAllocator;
    ComPtr<ID3D12GraphicsCommandList> _cmdList;
    std::shared_ptr<DXSwapchain> _swapchain{};

    // 主循环状态机
    bool _recording{false};       // cmdlist 处于可录制态
    bool _rtActive{false};        // 当前 OM 目标已激活（endFrame 显式清零）
    bool _omPending{false};       // RT 变更待落地（OMSetRenderTargets+Clear）
    bool _backBufferBound{false}; // 当前 backbuffer 已转 RENDER_TARGET（防重复屏障）

    // 渲染状态
    std::shared_ptr<IPipeline> _pipeline{};
    std::array<std::shared_ptr<IBuffer>, 16> _vertexBuffers{};
    std::shared_ptr<IBuffer> _indexBuffer{};
    float _clearColor[4]{0.0f, 0.0f, 0.0f, 1.0f};
    // clear 色按渲染目标分别记录（key=IRenderTarget*，swapchain 用 nullptr），
    // 对齐 GL/VK 语义：clearColor 只影响"调用时绑定"的目标，避免多 pass 交叉污染
    std::map<void*, std::array<float, 4>> _clearColors{};
    IBuffer* _uniformBuffer{nullptr};   // 最近创建的 UBO（App 每样例一个，resetRenderState 清除）
    bool _viewportSet{false};
};

bool DXRenderer::init(const std::shared_ptr<ISurface>& surface) {
    _surface = surface;
    // IID_PPV_ARGS 宏展开为逗号分隔的两个实参，不能放进三目表达式；
    // 调试层启用决策延后（Windows 原生可用，留后续任务验证期开启）。
    ComPtr<ID3D12Debug> dbg;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) { dbg.ptr->Release(); dbg.ptr = nullptr; }
    DX_CHECK(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&_device)), "create device");
    if (!_device.ptr) return false;
    D3D12_COMMAND_QUEUE_DESC qd{}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    DX_CHECK(_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&_queue)), "create queue");
    DX_CHECK(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_frameFence)), "fence");
    _fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    DX_CHECK(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_uploadAllocator)),
             "create upload allocator");
    if (!CreateSharedRootSignature(_device.ptr, &_rootSignature)) return false;

    // 帧录制资源。CreateCommandList 返回即处于录制态，须先 Close 才能走统一的 Reset 流程
    DX_CHECK(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                             IID_PPV_ARGS(&_frameAllocator)),
             "create frame allocator");
    if (!_frameAllocator.ptr) return false;
    DX_CHECK(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _frameAllocator.ptr,
                                        nullptr, IID_PPV_ARGS(&_cmdList)),
             "create frame command list");
    if (!_cmdList.ptr) return false;
    _cmdList.ptr->Close();

    _swapchain = std::make_shared<DXSwapchain>();
    if (!_swapchain->init(_device.ptr, _queue.ptr, _frameFence.ptr, _fenceEvent, surface)) {
        LOGE("[DX12] swapchain init failed");
        return false;
    }
    LOGI("[DX12] device ready");
    return true;
}

void DXRenderer::shutdown() {
    waitForGpuIdle();
    _uniformBuffer = nullptr;
    _pipeline = nullptr;
    _indexBuffer = nullptr;
    _vertexBuffers = {};
    _renderTarget = nullptr;
    _clearColors.clear();
    _swapchain.reset();
    if (_cmdList.ptr) { _cmdList.ptr->Release(); _cmdList.ptr = nullptr; }
    if (_frameAllocator.Get()) { _frameAllocator->Release(); _frameAllocator.ptr = nullptr; }
    if (_fenceEvent) { CloseHandle(_fenceEvent); _fenceEvent = nullptr; }
    if (_uploadAllocator.Get()) { _uploadAllocator->Release(); _uploadAllocator.ptr = nullptr; }
    if (_rootSignature.Get()) { _rootSignature->Release(); _rootSignature.ptr = nullptr; }
    if (_frameFence.Get()) { _frameFence->Release(); _frameFence.ptr = nullptr; }
    if (_queue.Get()) { _queue->Release(); _queue.ptr = nullptr; }
    if (_device.Get()) { _device->Release(); _device.ptr = nullptr; }
    _surface.reset();
    _recording = false;
    _rtActive = false;
    _omPending = false;
    _backBufferBound = false;
}

// pending RT 已在 present 前落地；此处收尾屏障（RENDER_TARGET→PRESENT）、关单、
// 提交、呈现、fence 单调推进等待，最后重置 cmdlist 开录下帧
bool DXRenderer::present() {
    if (!_recording || !_cmdList.ptr || !_swapchain || !_swapchain->initialized()) return false;
    if (_omPending) flushOmTargets();   // 无 draw 的帧也保证背景清屏已录制
    if (_backBufferBound) {
        ID3D12Resource* bb = _swapchain->backBuffer(_swapchain->currentIndex());
        if (bb) {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = bb;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            _cmdList.ptr->ResourceBarrier(1, &barrier);
        }
        _backBufferBound = false;
    }
    DX_CHECK(_cmdList->Close(), "close frame command list");
    _recording = false;
    ID3D12CommandList* lists[] = {_cmdList.ptr};
    _queue->ExecuteCommandLists(1, lists);
    const bool ok = _swapchain->present();   // Present(1,0)
    _swapchain->waitForGpuIdle();            // MoveToNextFrame：等 GPU 完成后方可复用 allocator
    DX_CHECK(_frameAllocator->Reset(), "reset frame allocator");
    DX_CHECK(_cmdList->Reset(_frameAllocator.ptr, nullptr), "reset frame command list");
    _recording = true;
    _rtActive = false;
    return ok;
}

bool DXRenderer::prepareDraw(bool needsIndex) {
    if (!_device.ptr || !_recording || !_cmdList.ptr) return false;
    auto dxp = std::dynamic_pointer_cast<DXPipeline>(_pipeline);
    if (!dxp) return false;
    flushOmTargets();

    // PSO 取用：key 含当前 RT 格式布局/采样数（Task 6 仅窗口路径）
    PSOKey key{};
    key.stateHash = dxp->stateHash();
    key.colorCount = 1;
    key.color[0] = _swapchain ? _swapchain->colorFormat() : DXGI_FORMAT_R8G8B8A8_UNORM;
    key.depth = DXGI_FORMAT_D24_UNORM_S8_UINT;
    key.samples = 1;
    ID3D12PipelineState* pso = dxp->pipelineFor(key);
    if (!pso) return false;

    _cmdList.ptr->SetGraphicsRootSignature(_rootSignature.ptr);
    _cmdList.ptr->SetPipelineState(pso);
    // SRV 描述符堆与根描述符表（param1）随 Task 7 接入：本任务无纹理采样，
    // 不绑 heap/表即合法（shader 不读 t 寄存器）

    // param0 根 CBV：直挂当前 UBO ring 槽 GPU VA。App 在 draw 前调 update() 推进
    // DXBuffer 内部 ring 头，这里读 submittedBase() 即得本次数据的槽基址；
    // 帧间经 fence 全串行化，ring 跨帧回绕安全，无需 renderer 侧槽计数器
    auto ubo = std::dynamic_pointer_cast<DXBuffer>(_uniformBuffer);
    if (ubo && ubo->handle()) {
        auto* res = static_cast<ID3D12Resource*>(ubo->handle());
        _cmdList.ptr->SetGraphicsRootConstantBufferView(
            0, res->GetGPUVirtualAddress() +
                   static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(ubo->submittedBase()));
    }

    _cmdList.ptr->IASetPrimitiveTopology(ToDxTopology(dxp->primitiveType()));

    // VB 按 layout.binding 分槽装配，stride 取自该 binding 的顶点元素步长
    const VertexLayout& layout = dxp->layout();
    for (uint32_t slot = 0; slot < _vertexBuffers.size(); ++slot) {
        auto vb = std::dynamic_pointer_cast<DXBuffer>(_vertexBuffers[slot]);
        if (!vb || !vb->handle()) continue;
        uint32_t stride = 0;
        for (const auto& e : layout.elements) {
            if (static_cast<uint32_t>(e.binding) == slot && e.stride > 0) {
                stride = static_cast<uint32_t>(e.stride);
                break;
            }
        }
        vb->BindAsVB(_cmdList.ptr, slot, stride);
    }
    if (needsIndex) {
        auto ib = std::dynamic_pointer_cast<DXBuffer>(_indexBuffer);
        if (!ib || !ib->handle()) {
            WarnOnce("drawIndexed without a bound index buffer; draw skipped");
            return false;
        }
        ib->BindAsIB(_cmdList.ptr);
    }
    return true;
}

std::shared_ptr<IRenderer> createDX12Renderer() { return std::make_shared<DXRenderer>(); }

} // namespace rhi
