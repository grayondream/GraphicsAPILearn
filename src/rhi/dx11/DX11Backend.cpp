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
        auto pl = std::make_shared<DX11Pipeline>(_device.ptr, layout, dxShader);
        return pl;
    }
    std::shared_ptr<IBuffer> createBuffer() override {
        if (!_device.ptr) { LOGE("[DX11] createBuffer before init"); return std::make_shared<DXNullBuffer>(); }
        return std::make_shared<DX11Buffer>(_device.ptr, _context.ptr);
    }
    std::shared_ptr<ITexture2D> createTexture2D() override {
        if (!_device.ptr) { LOGE("[DX11] createTexture2D before init"); return std::make_shared<DXNullTexture2D>(); }
        return std::make_shared<DX11Texture2D>(_device.ptr, _context.ptr);
    }
    std::shared_ptr<IBuffer> createUniformBuffer() override {
        if (!_device.ptr) { LOGE("[DX11] createUniformBuffer before init"); return std::make_shared<DXNullBuffer>(); }
        auto buf = std::make_shared<DX11Buffer>(_device.ptr, _context.ptr);
        // 每个样例持且仅持一个 UniformBlock 缓冲：最近创建者即当前绑定目标，
        // prepareDraw 时挂 cbuffer b0 槽（同 DX12 的 _uniformBuffer 跟踪约定）
        _uniformBuffer = buf;
        return buf;
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
        BindTexture2D(texture.get(), unit);
    }
    void bindTexture(const std::shared_ptr<ITexture3D>& texture, unsigned int unit) override {
        (void)texture; (void)unit;
        WarnOnce("bindTexture: cubemap binding not implemented yet (later task)");
    }
    void bindTexture(rhi::ITexture2D* texture, unsigned int unit) override {
        BindTexture2D(texture, unit);
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
        _srvSlots.fill(nullptr);   // 纹理 SRV 槽位随上一样例销毁，先清防悬垂
        // 采样器换装复位白档（上一样例的黑边框绑定不得泄漏到下一样例）
        for (uint32_t i = 0; i < kSamplerSlots; ++i) _activeSamplers[i] = _samplers[i].Get();
        _windowBound = false;
        _viewportSet = false;
        _viewport = {};
    }
    void waitIdle() override {}   // 即时上下文命令同步执行，无需等待
    void flush() override {}

private:
    // GL 默认状态对齐（GL 默认深度关/剔除关/混合关）：窗口目标激活后统一下发。
    // prepareDraw 按管线状态对象每 draw 覆盖，本函数保证 draw 前的 clear 阶段
    // 不受 D3D11 出厂默认（CullMode=BACK、DepthEnable=TRUE）影响
    void createDefaultStates();

    // 采样器全槽预建（Task 2.4）：0-8=f*3+w 组合（border 白）、9=比较采样器
    // LESS_EQUAL+Border 白（勿动：阴影越界=最远=受光）、10/11=LOD bias 0 预留；
    // 另预建 border 组合的黑边框档（评审 Important-1，按 borderColor 精确选装）
    bool createSamplers();

    // bindTexture 公共实现：纹理 SRV 写入共享槽位 unit+1（槽 0 预留 ImGui，
    // 与 HLSL t<unit+1> 寄存器约定一致）；prepareDraw 每 draw 全量 PSSetShaderResources
    void BindTexture2D(rhi::ITexture2D* texture, unsigned int unit);
    // ClampToBorder 纹理按 borderColor 白/黑精确选装对应寄存器位的采样器状态
    // （D3D11 采样器状态不可变+HLSL s# 寄存器静态绑定，同 draw 同组合异色仍不可
    // 表达——与 DX12 动态采样器堆的已记录限制一致）；非白/黑 WARN 回退白
    void InstallBorderColorSampler(const TextureDesc& params, const float bc[4]);

    // OMSetRenderTargets(窗口 RTV+DSV) → 默认状态 → 可选清屏 → 视口
    void bindWindowTargets(bool doClear);
    void clearWindowTargets();

    // 正高度视口直绘（D3D NDC y-up，勿照搬 VK 负高度翻转——Rect 曾因此整体镜像）
    void applyViewport();

    // 公共绘制前奏（Task 2.2 全量重设，实现见下方定义处注释）
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

    // 采样器全槽预建（寄存器编号=f*3+w 与 res/DX11/_samplers.hlsli 别名一致）+
    // border 黑档（评审 Important-1：brief"白/黑两档"落地）+ 纹理 SRV 共享槽位表
    // （槽 0 预留 ImGui，槽 i ↔ t<i>）。_activeSamplers 为每 draw 实际下发的运行时
    // 表：默认=白档，ClampToBorder 纹理按 borderColor 换装黑档（resetRenderState 复位）
    static constexpr uint32_t kSamplerSlots = 12;   // 0..8 组合 + s9 比较 + s10/s11 预留
    static constexpr uint32_t kBlackBorderSlots = 3; // 三个 filter 的黑边框变体（非 s# 寄存器）
    static constexpr uint32_t kSrvSlots = 16;       // t0 预留 ImGui，t1..t15 = unit 0..14
    std::array<Dx11ComPtr<ID3D11SamplerState>, kSamplerSlots> _samplers{};
    std::array<Dx11ComPtr<ID3D11SamplerState>, kBlackBorderSlots> _samplersBlack{};
    std::array<ID3D11SamplerState*, kSamplerSlots> _activeSamplers{};
    std::array<ID3D11ShaderResourceView*, kSrvSlots> _srvSlots{};

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
    if (!createSamplers()) {
        LOGE("[DX11] sampler creation failed");
        return false;
    }
    // TEMP 边框采样器选装自检（评审 Important-1 验证用，RHI_DX11_BORDERTEST=1 触发）
    if (std::getenv("RHI_DX11_BORDERTEST")) {
        TextureDesc td;
        td.wrapS = td.wrapT = td.wrapR = TextureWrap::ClampToBorder;
        const std::array<float, 4> white{{1.0f, 1.0f, 1.0f, 1.0f}};
        const std::array<float, 4> black{{0.0f, 0.0f, 0.0f, 1.0f}};
        const std::array<float, 4> gray{{0.5f, 0.5f, 0.5f, 1.0f}};
        td.minFilter = TextureFilter::Linear;
        InstallBorderColorSampler(td, white.data());
        LOGI("[DX11][BT] linear+white   s2={} expect-white={}", static_cast<void*>(_activeSamplers[2]), static_cast<void*>(_samplers[2].Get()));
        InstallBorderColorSampler(td, black.data());
        LOGI("[DX11][BT] linear+black   s2={} expect-black={}", static_cast<void*>(_activeSamplers[2]), static_cast<void*>(_samplersBlack[0].Get()));
        td.minFilter = TextureFilter::Nearest;
        InstallBorderColorSampler(td, black.data());
        LOGI("[DX11][BT] nearest+black  s5={} expect-black={}", static_cast<void*>(_activeSamplers[5]), static_cast<void*>(_samplersBlack[1].Get()));
        InstallBorderColorSampler(td, gray.data());
        LOGI("[DX11][BT] nearest+gray   s5={} keep-black={} (WARN expected above)", static_cast<void*>(_activeSamplers[5]), static_cast<void*>(_samplersBlack[1].Get()));
        td.minFilter = TextureFilter::LinearMipLinear;
        InstallBorderColorSampler(td, black.data());
        LOGI("[DX11][BT] miplinear+black s8={} expect-black={}", static_cast<void*>(_activeSamplers[8]), static_cast<void*>(_samplersBlack[2].Get()));
        InstallBorderColorSampler(td, white.data());
        LOGI("[DX11][BT] miplinear+white  s8={} expect-white={}", static_cast<void*>(_activeSamplers[8]), static_cast<void*>(_samplers[8].Get()));
    }

    _swapchain = std::make_shared<DX11Swapchain>();
    if (!_swapchain->init(_device.ptr, surface)) {
        LOGE("[DX11] swapchain init failed");
        return false;
    }
    LOGI("[DX11] device ready fl=0x{:X}", static_cast<unsigned>(_featureLevel));
    return true;
}

// 采样器全槽预建（Task 2.4 + 评审 Important-1 黑档）。槽位布局（寄存器编号与
// _samplers.hlsli 别名一致）：
//   0..8 = f*3+w 组合（f: Linear=0/Nearest=1/LinearMipLinear=2 × w: Repeat=0/
//          ClampToEdge=1/ClampToBorder=2），border 槽位预填白边框；
//   9    = 硬件比较采样器 COMPARISON_MIN_MAG_MIP_POINT + LESS_EQUAL + Border 白
//          （shadow map 专用 gShadowCompare；白边框勿动——越界=far=受光语义）；
//   10/11= LinearMipLinear+Repeat 的 LOD bias 占位（DX12 为 NVIDIA 隐式 LOD 对齐
//          设 bias 0.28/0.85；brief 指定 DX11 预留 0）；
//   另建 _samplersBlack[3] = 三个 filter 的黑边框变体（brief"border 白/黑两档"），
//   经 InstallBorderColorSampler 按 borderColor 精确换装到对应 s# 寄存器位。
// D3D11 采样器状态不可变：仅白/黑两档可静态表达，其余色 WARN 回退白（动态色延后）。
bool DX11Renderer::createSamplers() {
    constexpr TextureFilter filters[3] = {TextureFilter::Linear, TextureFilter::Nearest,
                                          TextureFilter::LinearMipLinear};
    for (int slot = 0; slot < 9; ++slot) {
        const int f = slot / 3;
        const int w = slot % 3;
        const TextureWrap wrap = static_cast<TextureWrap>(w);
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = Dx11FilterOf(filters[f]);
        sd.AddressU = Dx11AddressOf(wrap);
        sd.AddressV = Dx11AddressOf(wrap);
        sd.AddressW = Dx11AddressOf(wrap);
        sd.MipLODBias = 0.0f;
        sd.MaxAnisotropy = 1;   // 非各向异性过滤时必须为 1
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        // border 槽位预填白（越界=最远深度=受光，阴影采样依赖此语义）
        const bool border = wrap == TextureWrap::ClampToBorder;
        sd.BorderColor[0] = border ? 1.0f : 0.0f;
        sd.BorderColor[1] = border ? 1.0f : 0.0f;
        sd.BorderColor[2] = border ? 1.0f : 0.0f;
        sd.BorderColor[3] = border ? 1.0f : 0.0f;
        sd.MinLOD = 0;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        DX11_CHECK(_device->CreateSamplerState(&sd, &_samplers[slot]), "create sampler state");
        if (!_samplers[slot].Get()) return false;
    }
    {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.MipLODBias = 0.0f;
        sd.MaxAnisotropy = 1;
        sd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        sd.BorderColor[0] = 1.0f;   // 越界=far=受光
        sd.BorderColor[1] = 1.0f;
        sd.BorderColor[2] = 1.0f;
        sd.BorderColor[3] = 1.0f;
        sd.MinLOD = 0;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        DX11_CHECK(_device->CreateSamplerState(&sd, &_samplers[9]), "create comparison sampler");
        if (!_samplers[9].Get()) return false;
    }
    for (int slot : {10, 11}) {   // LOD bias 预留位（bias=0）
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = Dx11FilterOf(TextureFilter::LinearMipLinear);
        sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.MipLODBias = 0.0f;
        sd.MaxAnisotropy = 1;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MinLOD = 0;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        DX11_CHECK(_device->CreateSamplerState(&sd, &_samplers[slot]),
                   "create reserved sampler");
        if (!_samplers[slot].Get()) return false;
    }
    // 黑边框档：三个 filter 各一（对应寄存器 s2/s5/s8），供 borderColor=黑时换装
    for (int f = 0; f < 3; ++f) {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = Dx11FilterOf(filters[f]);
        sd.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.MipLODBias = 0.0f;
        sd.MaxAnisotropy = 1;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.BorderColor[0] = 0.0f;
        sd.BorderColor[1] = 0.0f;
        sd.BorderColor[2] = 0.0f;
        sd.BorderColor[3] = 1.0f;   // GL 黑边框默认 alpha=1（Common.hpp 同口径）
        sd.MinLOD = 0;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        DX11_CHECK(_device->CreateSamplerState(&sd, &_samplersBlack[f]),
                   "create black-border sampler");
        if (!_samplersBlack[f].Get()) return false;
    }
    // 运行时下发表初始=白档
    for (uint32_t i = 0; i < kSamplerSlots; ++i) _activeSamplers[i] = _samplers[i].Get();
    return true;
}

// ClampToBorder 纹理按 borderColor 白/黑精确换装对应寄存器位的采样器状态。
// 寄存器编号沿用 f*3+w 约定（w 取 wrapS，同 DX12 TouchHeapSampler 口径）；
// 换装持久生效至下一次异色绑定或 resetRenderState 复位白档——同一 draw 内同组合
// 异色不可表达（s# 寄存器静态绑定，与 DX12 动态采样器堆的已记录限制一致）。
void DX11Renderer::InstallBorderColorSampler(const TextureDesc& params, const float bc[4]) {
    static const std::array<float, 4> kWhite{{1.0f, 1.0f, 1.0f, 1.0f}};
    static const std::array<float, 4> kBlack{{0.0f, 0.0f, 0.0f, 1.0f}};
    const bool white = std::memcmp(bc, kWhite.data(), sizeof(float) * 4) == 0;
    const bool black = std::memcmp(bc, kBlack.data(), sizeof(float) * 4) == 0;
    if (!white && !black) {
        WarnOnce("borderColor not white/black unsupported by static sampler table; using white");
        return;   // 保持当前档位不动（预填语义为白）
    }
    int fi = 0;
    switch (params.minFilter) {
        case TextureFilter::Linear:          fi = 0; break;
        case TextureFilter::Nearest:         fi = 1; break;
        case TextureFilter::LinearMipLinear: fi = 2; break;
    }
    const UINT reg = static_cast<UINT>(fi * 3 + 2);   // w=2 即 border 组合位
    ID3D11SamplerState* want =
        black ? _samplersBlack[static_cast<size_t>(fi)].Get() : _samplers[reg].Get();
    if (_activeSamplers[reg] != want) {
        LOGI("[DX11] border sampler s{} -> {}", reg, black ? "black" : "white");
        _activeSamplers[reg] = want;
    }
}

// bindTexture 公共实现（对照 DX12 BindTexture2D）：SRV 写共享槽位 unit+1，
// 槽 0 预留 ImGui。SRV 由纹理对象 init 时创建并持有，此处仅借用指针——
// 样例切换经 resetRenderState 清槽防悬垂。
void DX11Renderer::BindTexture2D(rhi::ITexture2D* texture, unsigned int unit) {
    if (!texture) return;
    // 64 位中间量防 unit=UINT_MAX 时 unit+1 回绕绕过守卫（评审 Minor-4）
    if (static_cast<uint64_t>(unit) + 1 >= kSrvSlots) {
        WarnOnce("bindTexture unit out of SRV slot range; ignored");
        return;
    }
    auto* tex = dynamic_cast<DX11Texture2D*>(texture);
    if (!tex || !tex->valid()) return;
    if (!tex->srv()) {
        WarnOnce("bindTexture: texture has no SRV (MSAA RTV-only?); ignored");
        return;
    }
    _srvSlots[unit + 1] = tex->srv();
    // ClampToBorder 纹理按 borderColor 白/黑精确选装采样器档位（评审 Important-1：
    // 黑色不再静默得白边框）；其余色在 Install 内 WARN 回退白
    const TextureDesc& params = tex->samplerParams();
    if (params.wrapS == TextureWrap::ClampToBorder ||
        params.wrapT == TextureWrap::ClampToBorder ||
        params.wrapR == TextureWrap::ClampToBorder) {
        InstallBorderColorSampler(params, tex->borderColor().data());
    }
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
    _srvSlots.fill(nullptr);
    for (auto& s : _samplers) s.Reset();
    for (auto& s : _samplersBlack) s.Reset();
    _activeSamplers.fill(nullptr);
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

// 公共绘制前奏（Task 2.2 draw 路径全量）：每次 draw 全量重设 OM 目标/状态对象/
// 视口/着色器/输入装配/cbuffer——DX11 即时上下文无状态泄漏风险，对齐 DX12
// "prepareDraw 自愈重设" 注释语义。顺序要点：OMSetRenderTargets 会解绑与 RT
// 冲突的 SRV，故 SRV 绑定必须在其之后。
bool DX11Renderer::prepareDraw(bool needsIndex) {
    if (!_context.ptr || !_pipeline) return false;
    auto p = std::dynamic_pointer_cast<DX11Pipeline>(_pipeline);
    if (!p || !p->valid()) return false;

    // OM 目标自愈重绑（窗口路径；离屏 RT Task 3 接入后在此分支扩展）
    if (_windowBound && _swapchain && _swapchain->initialized()) {
        const uint32_t idx = _swapchain->currentIndex();
        ID3D11RenderTargetView* rtv = _swapchain->acquireRtv(idx);
        ID3D11DepthStencilView* dsv = _swapchain->dsv();
        if (rtv) {
            ID3D11RenderTargetView* rtvs[1] = {rtv};
            _context->OMSetRenderTargets(1, rtvs, dsv);
        }
    }

    p->bindShaders(_context.ptr);
    if (!p->bindStates(_context.ptr)) {
        // RS/DS/Blend 状态对象创建失败（不缓存不重试）：跳过本次 draw，
        // 对照 DX12 pipelineFor 返回 nullptr 时 prepareDraw 直接退出的语义
        return false;
    }
    applyViewport();               // RSSetViewports 每 draw 重发

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

    // 纹理采样：SRV 槽位 t<unit+1>（槽 0 预留 ImGui）+ 采样器全槽绑定
    // （_activeSamplers=白档预填表，ClampToBorder 黑边框纹理经换装生效）。
    // 本仓库全部纹理采样在 PS（同 DX12 根表 PIXEL 可见性口径）
    _context->PSSetShaderResources(0, kSrvSlots, _srvSlots.data());
    _context->PSSetSamplers(0, kSamplerSlots, _activeSamplers.data());
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
