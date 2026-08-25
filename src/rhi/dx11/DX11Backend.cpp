#include "rhi/dx11/DX11Backend.hpp"
#include "rhi/core/ISurface.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/ITexture3D.hpp"
#include "rhi/core/IRenderTarget.hpp"

#include <array>
#include <cstdio>    // std::fopen/fprintf（RHI_DUMP_FRAME PPM 导出）
#include <cstdlib>   // std::getenv
#include <map>

#ifndef RESOURCE_DIR
#define RESOURCE_DIR "res"
#endif

namespace rhi {

// 只警告一次的桩实现提示（纹理/RT 绑定属后续任务范围）
static void WarnOnce(const char* message) {
    struct Flag { bool done{false}; };
    static std::map<const char*, Flag> warned;
    auto& f = warned[message];
    if (!f.done) {
        f.done = true;
        LOGW("[DX11] {}", message);
    }
}

// ---- 设备侧诊断（DX11Header.hpp 声明，dx11diag 命名空间避免与 DX12 的
// rhi::dxdiag 同签名符号 LNK2005 冲突）----
namespace {

Dx11ComPtr<ID3D11InfoQueue>& DxDiagQueue() {
    static Dx11ComPtr<ID3D11InfoQueue> q;
    return q;
}

} // namespace

void dx11diag::SetInfoQueue(ID3D11Device* device) {
    if (!device) return;
    Dx11ComPtr<ID3D11InfoQueue> got;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&got)))) {
        DxDiagQueue() = std::move(got);
    }
}

void dx11diag::DumpMessages(const char* context) {
    Dx11ComPtr<ID3D11InfoQueue>& iq = DxDiagQueue();
    if (!iq.ptr) return;
    const UINT64 count = iq->GetNumStoredMessages();
    for (UINT64 i = 0; i < count && i < 32; ++i) {
        SIZE_T len = 0;
        if (FAILED(iq->GetMessage(i, nullptr, &len)) || len == 0) continue;
        std::vector<char> buf(len);
        auto* msg = reinterpret_cast<D3D11_MESSAGE*>(buf.data());
        if (SUCCEEDED(iq->GetMessage(i, msg, &len)) && msg->pDescription) {
            LOGE("[DX11][dq] {}: {}", context, msg->pDescription);
        }
    }
    iq->ClearStoredMessages();
}

namespace {

// Null 桩仅作 init 前兜底（create()==false 使样例在加载期经 ExitIfFailed 干净
// 退出，避免空指针解引用段错误）；真实工厂在 device 就绪后返回 DX 实现类。

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

// VB/IB/UBO 真实现（Task 1 上屏必需）：DEFAULT 堆一次性上传，更新走
// UpdateSubresource。常量缓冲要求 ByteWidth 为 16 的倍数且不允许部分 box 更新。
class DX11Buffer : public IBuffer {
public:
    DX11Buffer(ID3D11Device* device, ID3D11DeviceContext* ctx)
        : _device(device), _ctx(ctx) {}

    bool init(const void* data, size_t size, BufferType type) override {
        if (!_device || size == 0) return false;
        _type = type;
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = static_cast<UINT>(size);
        switch (type) {
            case BufferType::Vertex:  bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; break;
            case BufferType::Index:   bd.BindFlags = D3D11_BIND_INDEX_BUFFER; break;
            case BufferType::Uniform:
                bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                // 常量缓冲 ByteWidth 必须 16 的倍数（上限 65536 由调用方保证）
                bd.ByteWidth = (bd.ByteWidth + 15u) & ~15u;
                break;
        }
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.CPUAccessFlags = 0;
        bd.MiscFlags = 0;
        bd.StructureByteStride = 0;
        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = data;
        sd.SysMemPitch = 0;
        sd.SysMemSlicePitch = 0;
        DX11_CHECK(_device->CreateBuffer(&bd, data ? &sd : nullptr, &_buffer),
                   "create buffer");
        _byteWidth = bd.ByteWidth;
        return _buffer.Get() != nullptr;
    }

    bool update(const void* data, size_t size, size_t offset) override {
        if (!_buffer.Get() || !_ctx || !data || size == 0) return false;
        if (offset + size > _byteWidth) {
            LOGE("[DX11] buffer update out of range offset={} size={} width={}",
                 offset, size, _byteWidth);
            return false;
        }
        if (_type == BufferType::Uniform || (offset == 0 && size == _byteWidth)) {
            // 常量缓冲不允许部分 box：整块提交
            _ctx->UpdateSubresource(_buffer.Get(), 0, nullptr, data, 0, 0);
        } else {
            D3D11_BOX box{};
            box.left = static_cast<UINT>(offset);
            box.right = static_cast<UINT>(offset + size);
            box.top = 0;
            box.bottom = 1;
            box.front = 0;
            box.back = 1;
            _ctx->UpdateSubresource(_buffer.Get(), 0, &box, data, 0, 0);
        }
        return true;
    }

    bool bindRange(uint32_t, size_t, size_t) override { return true; }   // b0 槽位 draw 时统一绑定
    bool bind() override { return true; }
    void* handle() override { return _buffer.Get(); }

private:
    ID3D11Device* _device{nullptr};
    ID3D11DeviceContext* _ctx{nullptr};
    Dx11ComPtr<ID3D11Buffer> _buffer{};
    BufferType _type{BufferType::Vertex};
    UINT _byteWidth{0};
};

} // namespace

class DX11Renderer : public IRenderer {
public:
    ~DX11Renderer() override { shutdown(); }

    bool init(const std::shared_ptr<ISurface>& surface) override;
    void shutdown() override;

    // ---- 资源工厂 ----
    std::shared_ptr<IShader> createShader() override { return std::make_shared<DX11Shader>(); }
    std::shared_ptr<IPipeline> createPipeline(const VertexLayout& layout, const std::shared_ptr<IShader>& shader) override {
        auto dxShader = std::dynamic_pointer_cast<DX11Shader>(shader);
        if (!_device.ptr || !dxShader || !dxShader->valid()) {
            LOGE("[DX11] createPipeline: device/shader not ready, null fallback");
            return std::make_shared<DXNullPipeline>();
        }
        return std::make_shared<DX11Pipeline>(_device.ptr, layout, dxShader);
    }
    std::shared_ptr<IBuffer> createBuffer() override {
        if (!_device.ptr) { LOGE("[DX11] createBuffer before init"); return std::make_shared<DXNullBuffer>(); }
        return std::make_shared<DX11Buffer>(_device.ptr, _context.ptr);
    }
    std::shared_ptr<IBuffer> createUniformBuffer() override {
        if (!_device.ptr) { LOGE("[DX11] createUniformBuffer before init"); return std::make_shared<DXNullBuffer>(); }
        auto buf = std::make_shared<DX11Buffer>(_device.ptr, _context.ptr);
        // 每个样例持且仅持一个 UniformBlock 缓冲：最近创建者即当前绑定目标，
        // prepareDraw 时挂 cbuffer b0 槽（同 DX12 的 _uniformBuffer 跟踪约定）
        _uniformBuffer = buf;
        return buf;
    }
    std::shared_ptr<ITexture2D> createTexture2D() override {
        WarnOnce("createTexture2D: not implemented yet (later task); returning null stub");
        return std::make_shared<DXNullTexture2D>();
    }
    std::shared_ptr<ITexture3D> createTexture3D() override {
        WarnOnce("createTexture3D: not implemented yet (later task); returning null stub");
        return std::make_shared<DXNullTexture3D>();
    }
    std::shared_ptr<IRenderTarget> createRenderTarget() override {
        WarnOnce("createRenderTarget: not implemented yet (later task); returning null stub");
        return std::make_shared<DXNullRenderTarget>();
    }
    std::shared_ptr<ISwapchain> getSwapchain() override { return _swapchain; }

    // ---- 帧控制 ----
    void beginFrame() override {
        // 窗口目标绑定 + Clear RTV/DSV + 视口缓存下发（clearColor 语义同 GL 默认帧缓冲全清）
        if (!_swapchain || !_swapchain->initialized()) return;
        bindWindowTargets(true);
    }
    void endFrame() override {}   // 即时上下文无帧命令收尾（brief 指定空实现）
    bool present() override;

    // ---- 状态与绘制 ----
    void clearColor(float r, float g, float b, float a) override {
        _clearColor[0] = r; _clearColor[1] = g; _clearColor[2] = b; _clearColor[3] = a;
        // GL 语义：清"调用时绑定"的目标——窗口 RT 已绑定时立即生效（含深度/模板，
        // 同 GLBackend COLOR|DEPTH|STENCIL 全清）；beginFrame 的常规清屏不在此路径
        if (_windowBound) clearWindowTargets();
    }
    void setViewport(const Viewport& vp) override {
        _viewport = vp;
        _viewportSet = true;
        applyViewport();
    }
    void setPipeline(const std::shared_ptr<IPipeline>& pipeline) override { _pipeline = pipeline; }
    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer) override { setVertexBuffer(buffer, 0); }
    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer, uint32_t binding) override {
        if (binding < _vertexBuffers.size()) _vertexBuffers[binding] = buffer;
    }
    void setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) override { _indexBuffer = buffer; }
    void setRenderTarget(const std::shared_ptr<IRenderTarget>& target) override {
        // null=窗口路径：立即重绑并清屏（对齐 VK 每次 RP 重开 loadOp=Clear、
        // DX12 flushOmTargets(null)=activateWindowTargets(true) 的语义）；
        // 离屏 RT 属后续任务，先警告忽略
        if (!target) {
            if (_swapchain && _swapchain->initialized()) bindWindowTargets(true);
            return;
        }
        WarnOnce("setRenderTarget: offscreen targets not implemented yet; ignored");
    }
    void bindTexture(const std::shared_ptr<ITexture2D>& texture, unsigned int unit) override {
        (void)texture; (void)unit;
        WarnOnce("bindTexture: texture binding not implemented yet (later task)");
    }
    void bindTexture(const std::shared_ptr<ITexture3D>& texture, unsigned int unit) override {
        (void)texture; (void)unit;
        WarnOnce("bindTexture: cubemap binding not implemented yet (later task)");
    }
    void bindTexture(rhi::ITexture2D* texture, unsigned int unit) override {
        (void)texture; (void)unit;
        WarnOnce("bindTexture: texture binding not implemented yet (later task)");
    }
    void draw(uint32_t vertexCount, uint32_t firstVertex) override {
        if (!prepareDraw(false)) return;
        _context->Draw(vertexCount, firstVertex);
    }
    void drawIndexed(uint32_t indexCount, uint32_t indexOffset, uint32_t vertexOffset) override {
        if (!prepareDraw(true)) return;
        _context->DrawIndexed(indexCount, indexOffset, vertexOffset);
    }
    void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount,
                              uint32_t indexOffset, uint32_t vertexOffset) override {
        if (!prepareDraw(true)) return;
        _context->DrawIndexedInstanced(indexCount, instanceCount, indexOffset, vertexOffset, 0);
    }
    void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) override {
        if (!prepareDraw(false)) return;
        _context->DrawInstanced(vertexCount, instanceCount, firstVertex, 0);
    }
    void blitFramebuffer(const std::shared_ptr<IRenderTarget>& src,
                         const std::shared_ptr<IRenderTarget>& dst, BlitMask mask) override {
        (void)src; (void)dst; (void)mask;
        WarnOnce("blitFramebuffer: not implemented yet (later task)");
    }
    BackendCapabilities backendCapabilities() override {
        BackendCapabilities caps{};
        caps.maxUniformBlockSize = 64 * 1024;   // D3D11 常量缓冲单对象上限
        if (_device.Get()) {
            UINT quality = 0;
            for (UINT n : {UINT{8}, UINT{4}}) {
                quality = 0;
                if (SUCCEEDED(_device->CheckMultisampleQualityLevels(
                        DXGI_FORMAT_B8G8R8A8_UNORM, n, &quality)) &&
                    quality > 0) {
                    caps.maxSamples = static_cast<int>(n);
                    break;
                }
            }
        }
        return caps;
    }

    // ---- 样例切换 / 同步 ----
    void resetRenderState() override {
        // reloadSample 在 waitIdle 之后调用：清掉上一样例的资源引用（对齐 DX12/VK 行为）
        _pipeline = nullptr;
        _indexBuffer = nullptr;
        _vertexBuffers = {};
        _uniformBuffer = nullptr;
        _windowBound = false;
        _viewportSet = false;
        _viewport = {};
    }
    void waitIdle() override {}   // 即时上下文命令同步执行，无需等待
    void flush() override {}

private:
    // GL 默认状态对齐（GL 默认深度关/剔除关/混合关）：窗口目标激活后统一下发。
    // Task 3 状态对象化后由管线状态替代，本任务保证 Triangle/Rect 不受 D3D11
    // 出厂默认（CullMode=BACK、DepthEnable=TRUE）影响而误剔除/误遮挡
    void createDefaultStates();

    // OMSetRenderTargets(窗口 RTV+DSV) → 默认状态 → 可选清屏 → 视口
    void bindWindowTargets(bool doClear);
    void clearWindowTargets();

    // 正高度视口直绘（D3D NDC y-up，勿照搬 VK 负高度翻转——Rect 曾因此整体镜像）
    void applyViewport();

    // 公共绘制前奏：着色器/输入装配/拓扑/顶点缓冲/cbuffer b0
    bool prepareDraw(bool needsIndex);

    // RHI_DUMP_FRAME 帧导出（单次，对齐 DX11Renderer::dumpFrame）：staging 纹理读回
    // 当前 backbuffer 写 PPM(P6)。时序=present() 内、Present 之前；BGRA 内存序需
    // 读回换序为 RGB（DX12 为 RGBA8 直落盘，此处不同）
    void dumpFrame();

    std::shared_ptr<ISurface> _surface;
    Viewport _viewport{};
    bool _viewportSet{false};
    Dx11ComPtr<ID3D11Device> _device;
    Dx11ComPtr<ID3D11DeviceContext> _context;
    D3D_FEATURE_LEVEL _featureLevel{};
    std::shared_ptr<DX11Swapchain> _swapchain{};

    // GL 语义默认状态对象（生命周期与 device 一致）
    Dx11ComPtr<ID3D11RasterizerState> _rasterDefault;
    Dx11ComPtr<ID3D11DepthStencilState> _depthDefault;
    Dx11ComPtr<ID3D11BlendState> _blendDefault;

    // 主循环状态机
    bool _windowBound{false};     // 当前 OM 目标为窗口 backbuffer
    std::shared_ptr<IPipeline> _pipeline{};
    std::array<std::shared_ptr<IBuffer>, 16> _vertexBuffers{};
    std::shared_ptr<IBuffer> _indexBuffer{};
    std::shared_ptr<IBuffer> _uniformBuffer{};
    float _clearColor[4]{0.0f, 0.0f, 0.0f, 1.0f};

    // RHI_DUMP_FRAME 帧导出（一次性）
    bool _dumpDone{false};
};

bool DX11Renderer::init(const std::shared_ptr<ISurface>& surface) {
    _surface = surface;
    // BGRA 支持恒开（交换链格式 B8G8R8A8_UNORM 需要）；调试层经环境变量可选：
    // GRAPHICSLEARN_DX11_DEBUGLAYER=1 时启用，须先于建设备；SDK Layers 未安装时
    // 自动降级重试（创建失败不致命）
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    const bool wantDebug = std::getenv("GRAPHICSLEARN_DX11_DEBUGLAYER") != nullptr;
    if (wantDebug) flags |= D3D11_CREATE_DEVICE_DEBUG;

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                   nullptr, 0, D3D11_SDK_VERSION,
                                   &_device, &_featureLevel, &_context);
    if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG) != 0u) {
        LOGW("[DX11] debug-layer device unavailable hr=0x{:08X}; retrying without debug",
             static_cast<uint32_t>(hr));
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                               nullptr, 0, D3D11_SDK_VERSION,
                               &_device, &_featureLevel, &_context);
    }
    DX11_CHECK(hr, "create device");
    if (!_device.ptr || !_context.ptr) return false;
    dx11diag::SetInfoQueue(_device.ptr);
    if (_featureLevel < D3D_FEATURE_LEVEL_11_0) {
        LOGE("[DX11] feature level 11_0 unsupported (got 0x{:x})", static_cast<unsigned>(_featureLevel));
        return false;
    }

    createDefaultStates();

    _swapchain = std::make_shared<DX11Swapchain>();
    if (!_swapchain->init(_device.ptr, surface)) {
        LOGE("[DX11] swapchain init failed");
        return false;
    }
    LOGI("[DX11] device ready fl=0x{:X}", static_cast<unsigned>(_featureLevel));
    return true;
}

void DX11Renderer::createDefaultStates() {
    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = D3D11_FILL_SOLID;
    rs.CullMode = D3D11_CULL_NONE;   // GL 默认无剔除（Triangle/Rect 依赖此语义）
    rs.FrontCounterClockwise = TRUE; // GL 惯例 CCW 正面（CULL_NONE 下仅语义声明）
    rs.DepthBias = 0;
    rs.DepthBiasClamp = 0.0f;
    rs.SlopeScaledDepthBias = 0.0f;
    rs.DepthClipEnable = TRUE;
    rs.MultisampleEnable = FALSE;
    DX11_CHECK(_device->CreateRasterizerState(&rs, &_rasterDefault), "create default rasterizer state");

    D3D11_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = FALSE;          // GL 默认关闭深度测试
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    ds.DepthFunc = D3D11_COMPARISON_LESS;
    ds.StencilEnable = FALSE;
    ds.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
    ds.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    const D3D11_DEPTH_STENCILOP_DESC sop{D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP,
                                         D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS};
    ds.FrontFace = sop;
    ds.BackFace = sop;
    DX11_CHECK(_device->CreateDepthStencilState(&ds, &_depthDefault), "create default depth stencil state");

    D3D11_BLEND_DESC bs{};
    bs.AlphaToCoverageEnable = FALSE;
    bs.IndependentBlendEnable = FALSE;
    bs.RenderTarget[0].BlendEnable = FALSE;   // GL 默认关闭混合
    bs.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    bs.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    bs.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bs.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bs.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bs.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bs.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    DX11_CHECK(_device->CreateBlendState(&bs, &_blendDefault), "create default blend state");
}

void DX11Renderer::shutdown() {
    _uniformBuffer = nullptr;
    _pipeline = nullptr;
    _indexBuffer = nullptr;
    _vertexBuffers = {};
    _windowBound = false;
    _rasterDefault.Reset();
    _depthDefault.Reset();
    _blendDefault.Reset();
    if (_swapchain) _swapchain->shutdown();
    _swapchain.reset();
    if (_context.Get()) { _context->Release(); _context.ptr = nullptr; }
    if (_device.Get()) { _device->Release(); _device.ptr = nullptr; }
    _surface.reset();
}

bool DX11Renderer::present() {
    if (!_swapchain || !_swapchain->initialized()) return false;
    dumpFrame();   // 对齐 VK/DX12 口径：帧内容已入 backbuffer、Present 前
    return _swapchain->present();   // Present(1,0)
}

void DX11Renderer::bindWindowTargets(bool doClear) {
    if (!_swapchain || !_swapchain->initialized() || !_context.ptr) return;
    // flip 模型下 backbuffer 惰性就绪：当前槽未分配时跳过本帧，后续帧自动重试
    const uint32_t idx = _swapchain->currentIndex();
    ID3D11RenderTargetView* rtv = _swapchain->acquireRtv(idx);
    ID3D11DepthStencilView* dsv = _swapchain->dsv();
    if (!rtv) { LOGW("[DX11] window RTV not ready (index {}); frame skipped", idx); return; }
    ID3D11RenderTargetView* rtvs[1] = {rtv};
    _context->OMSetRenderTargets(1, rtvs, dsv);
    _context->RSSetState(_rasterDefault.Get());
    _context->OMSetDepthStencilState(_depthDefault.Get(), 0);
    _context->OMSetBlendState(_blendDefault.Get(), nullptr, 0xFFFFFFFFu);
    if (doClear) clearWindowTargets();
    applyViewport();
    _windowBound = true;
}

void DX11Renderer::clearWindowTargets() {
    ID3D11RenderTargetView* rtv = _swapchain->acquireRtv(_swapchain->currentIndex());
    ID3D11DepthStencilView* dsv = _swapchain->dsv();
    if (rtv) _context->ClearRenderTargetView(rtv, _clearColor);
    if (dsv) _context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

// 朝向约定（终验教训同 DX12）：D3D NDC y-up，swapchain 直绘用正高度视口即与 GL
// 呈现朝向一致。【历史】VK 式负高度翻转在 D3D 是错的——非 VK(y-down) 不需要，
// 会致 Triangle/Rect 等直绘图元整体 Y 镜像。
void DX11Renderer::applyViewport() {
    if (!_context.ptr) return;
    const int fbW = _swapchain ? _swapchain->width() : 0;
    const int fbH = _swapchain ? _swapchain->height() : 0;
    const float x = _viewportSet ? static_cast<float>(_viewport.x) : 0.0f;
    const float y = _viewportSet ? static_cast<float>(_viewport.y) : 0.0f;
    const float w = (_viewportSet && _viewport.width > 0)
                        ? static_cast<float>(_viewport.width)
                        : static_cast<float>(fbW);
    const float h = (_viewportSet && _viewport.height > 0)
                        ? static_cast<float>(_viewport.height)
                        : static_cast<float>(fbH);
    const D3D11_VIEWPORT vp{x, y, w, h, 0.0f, 1.0f};
    _context->RSSetViewports(1, &vp);
}

bool DX11Renderer::prepareDraw(bool needsIndex) {
    if (!_context.ptr || !_pipeline) return false;
    auto p = std::dynamic_pointer_cast<DX11Pipeline>(_pipeline);
    if (!p || !p->valid()) return false;

    p->bindShaders(_context.ptr);
    _context->IASetPrimitiveTopology(ToDx11Topology(p->primitiveType()));

    // VB 按 layout.binding 分槽装配，stride 取自该 binding 的顶点元素步长
    // （同 DX12 prepareDraw 的 BindAsVB stride 解析模式）
    constexpr uint32_t kSlots = 16;
    ID3D11Buffer* buffers[kSlots] = {};
    UINT strides[kSlots] = {};
    UINT offsets[kSlots] = {};
    const VertexLayout& layout = p->layout();
    for (uint32_t slot = 0; slot < kSlots; ++slot) {
        auto vb = _vertexBuffers[slot];
        if (!vb) continue;
        auto* b = static_cast<ID3D11Buffer*>(vb->handle());
        if (!b) continue;
        uint32_t stride = 0;
        for (const auto& e : layout.elements) {
            if (static_cast<uint32_t>(e.binding) == slot && e.stride > 0) {
                stride = static_cast<uint32_t>(e.stride);
                break;
            }
        }
        if (stride == 0) continue;
        buffers[slot] = b;
        strides[slot] = stride;
    }
    _context->IASetVertexBuffers(0, kSlots, buffers, strides, offsets);

    if (needsIndex) {
        auto ib = _indexBuffer;
        auto* b = ib ? static_cast<ID3D11Buffer*>(ib->handle()) : nullptr;
        if (!b) {
            WarnOnce("drawIndexed without a bound index buffer; draw skipped");
            return false;
        }
        _context->IASetIndexBuffer(b, DXGI_FORMAT_R32_UINT, 0);   // 全链路 unsigned int 索引
    }

    // 最近创建的 UBO 直挂 cbuffer b0（VS+PS 同槽；shader 未读取块时绑定无害）
    if (_uniformBuffer) {
        auto* cb = static_cast<ID3D11Buffer*>(_uniformBuffer->handle());
        if (cb) {
            ID3D11Buffer* cbs[1] = {cb};
            _context->VSSetConstantBuffers(0, 1, cbs);
            _context->PSSetConstantBuffers(0, 1, cbs);
        }
    }
    return true;
}

// RHI_DUMP_FRAME 帧导出：STAGING 纹理 CPU_READ 读回当前 backbuffer。交换链为
// B8G8R8A8_UNORM，内存字节序 B,G,R,A → 写 PPM 时换序为 RGB（VK/DX12 的 RGBA8
// 无需换序，此处口径差异已对齐 frame_compare 参照）。行序 top-down 与 VK/DX12
// dump 输出口径一致，无翻转。
void DX11Renderer::dumpFrame() {
    const char* dumpPath = std::getenv("RHI_DUMP_FRAME");
    if (!dumpPath || _dumpDone || !_swapchain || !_swapchain->initialized()) return;
    const char* skipEnv = std::getenv("RHI_DUMP_SKIP");
    if (skipEnv) {
        static int skipCount = 0;
        if (skipCount++ < std::atoi(skipEnv)) return;
    }
    _dumpDone = true;

    const int w = _swapchain->width();
    const int h = _swapchain->height();
    const uint32_t idx = _swapchain->currentIndex();
    if (!_swapchain->acquireRtv(idx)) {   // 顺带确保该槽 backbuffer 已分配
        LOGE("dumpFrame: swapchain backbuffer not ready");
        return;
    }
    ID3D11Texture2D* bb = _swapchain->backBuffer(idx);
    if (w <= 0 || h <= 0 || !bb || !_context.ptr) {
        LOGE("dumpFrame: swapchain backbuffer not ready");
        return;
    }

    D3D11_TEXTURE2D_DESC sd{};
    sd.Width = static_cast<UINT>(w);
    sd.Height = static_cast<UINT>(h);
    sd.MipLevels = 1;
    sd.ArraySize = 1;
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;   // 与 backbuffer 同格式（CopyResource 要求一致）
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    sd.MiscFlags = 0;
    Dx11ComPtr<ID3D11Texture2D> staging;
    DX11_CHECK(_device->CreateTexture2D(&sd, nullptr, &staging), "create dump staging texture");
    if (!staging.Get()) { LOGE("dumpFrame: create staging failed"); return; }

    _context->CopyResource(staging.Get(), bb);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = _context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr) || !mapped.pData) {
        LOGE("dumpFrame: map staging failed hr=0x{:08X}", static_cast<uint32_t>(hr));
        return;
    }
    const unsigned char* px = static_cast<const unsigned char*>(mapped.pData);
    std::string path = dumpPath;
    {
        FILE* fp = std::fopen(path.c_str(), "wb");
        if (!fp) {
            LOGE("dumpFrame: cannot open {}", path);
            _context->Unmap(staging.Get(), 0);
            return;
        }
        std::fprintf(fp, "P6\n%u %u\n255\n", static_cast<unsigned>(w), static_cast<unsigned>(h));
        for (int y = 0; y < h; y++) {
            const unsigned char* row = px + static_cast<size_t>(y) * mapped.RowPitch;
            for (int x = 0; x < w; x++) {
                std::fputc(row[x * 4 + 2], fp);   // R（BGRA 内存序换序）
                std::fputc(row[x * 4 + 1], fp);   // G
                std::fputc(row[x * 4 + 0], fp);   // B
            }
        }
        std::fclose(fp);
    }
    _context->Unmap(staging.Get(), 0);
    LOGI("dumpFrame: saved {} ({}x{})", path, w, h);

    uint32_t black = 0, nonblack = 0;
    for (int y = 0; y < h; y++) {
        const unsigned char* row = px + static_cast<size_t>(y) * mapped.RowPitch;
        for (int x = 0; x < w; x++) {
            const unsigned char* p = row + static_cast<size_t>(x) * 4;
            if (p[2] < 8 && p[1] < 8 && p[0] < 8) black++; else nonblack++;
        }
    }
    LOGI("dumpFrame: pixels black={} nonblack={}", black, nonblack);
}

std::shared_ptr<IRenderer> createDX11Renderer() { return std::make_shared<DX11Renderer>(); }

// ImGui overlay 初始化信息桥（Task 6 消费）：本任务按 brief 约定恒返回 false
bool GetDX11ImGuiInitInfo(const std::shared_ptr<IRenderer>& renderer, DX11ImGuiInitInfo& out) {
    (void)renderer;
    (void)out;
    return false;
}

} // namespace rhi
