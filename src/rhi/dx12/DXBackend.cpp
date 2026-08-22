#include "rhi/dx12/DXBackend.hpp"
#include "rhi/dx12/DXBuffer.hpp"
#include "rhi/dx12/DXPipeline.hpp"
#include "rhi/dx12/DXShader.hpp"
#include "rhi/dx12/DXSwapchain.hpp"
#include "rhi/dx12/DXTexture2D.hpp"
#include "rhi/core/ISurface.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/ITexture3D.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ISwapchain.hpp"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>

#ifndef RESOURCE_DIR
#define RESOURCE_DIR "res"
#endif

namespace rhi {

namespace {

// 离屏 RT 工厂暂返回 no-op 对象而非 nullptr：其 create()==false 使样例在
// 加载期经 ExitIfFailed 干净退出，避免空指针解引用段错误。Task 8 以同名真实类替换。

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

// 只警告一次的桩实现提示（ITexture3D 绑定属 Task 8 范围）
void WarnOnce(const char* message) {
    struct Flag { bool done{false}; };
    static std::map<const char*, Flag> warned;
    auto& f = warned[message];
    if (!f.done) {
        f.done = true;
        LOGW("[DX12] {}", message);
    }
}

// 读入 dxc 产物（DXShader::FindCso 同一查找约定）为 DXIL blob
ComPtr<ID3DBlob> LoadBlobFromCso(const std::string& sourcePath, ShaderStage::Type type) {
    namespace fs = std::filesystem;
    const std::string cso = DXShader::FindCso(sourcePath, type);
    std::ifstream f(cso, std::ios::binary);
    if (!f) return {};
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (bytes.empty() || bytes.size() % 4 != 0) return {};   // DXIL 容器按 32 位字组织
    ComPtr<ID3DBlob> blob;
    if (FAILED(D3DCreateBlob(bytes.size(), &blob)) || !blob.Get()) return {};
    std::memcpy(blob->GetBufferPointer(), bytes.data(), bytes.size());
    return blob;
}

// 内部 blit 专用 root signature：param0 = SRV 表 t0（PIXEL），静态采样器 s0 = Linear+Clamp。
// 独立于共享根签名（App 侧 t<unit+1>/s0..s8），供 mipmap 降采样与 RT↔RT 颜色 blit 复用
bool CreateBlitRootSignature(ID3D12Device* device, ComPtr<ID3D12RootSignature>& out) {
    D3D12_ROOT_PARAMETER param{};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable.NumDescriptorRanges = 1;
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    param.DescriptorTable.pDescriptorRanges = &range;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    sampler.MinLOD = 0;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 1;
    desc.pParameters = &param;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &errBlob);
    if (FAILED(hr)) {
        const char* msg = errBlob.Get() ? static_cast<const char*>(errBlob->GetBufferPointer()) : "";
        LOGE("[DX12] serialize blit root signature failed hr=0x{:08X} {}", static_cast<uint32_t>(hr), msg);
        return false;
    }
    DX_CHECK(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                         IID_PPV_ARGS(&out)),
             "create blit root signature");
    return out.Get() != nullptr;
}

namespace fs = std::filesystem;
fs::path DxSourcePath(const char* rel) {
    fs::path resRoot(RESOURCE_DIR);
    if (resRoot.is_relative()) resRoot = fs::absolute(resRoot);
    return resRoot / "DX12" / rel;
}

// 深度拷贝的 typed 格式（CopyTextureRegion 要求两端同格式；TYPELESS 资源给具体格式）
DXGI_FORMAT DepthCopyFormat(DXGI_FORMAT resourceFormat) {
    switch (resourceFormat) {
        case DXGI_FORMAT_R32_TYPELESS:   return DXGI_FORMAT_R32_FLOAT;
        case DXGI_FORMAT_R24G8_TYPELESS: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case DXGI_FORMAT_R16_TYPELESS:   return DXGI_FORMAT_R16_UNORM;
        default:                         return resourceFormat;
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
    DXRenderer() : _blitCtx(this) {}

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
    std::shared_ptr<ITexture2D> createTexture2D() override {
        if (!_device.ptr || !_srvHeap.ptr) {
            LOGE("[DX12] createTexture2D before init, null fallback");
            return std::make_shared<DXNullTexture2D>();
        }
        auto tex = std::make_shared<DXTexture2D>(_device.ptr, _queue.ptr,
                                                 _uploadAllocator.ptr, _frameFence.ptr,
                                                 _fenceEvent);
        tex->setBlitContext(&_blitCtx);
        return tex;
    }
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
    void bindTexture(const std::shared_ptr<ITexture2D>& texture, unsigned int unit) override {
        BindTexture2D(texture.get(), unit);
    }
    void bindTexture(const std::shared_ptr<ITexture3D>&, unsigned int) override {
        WarnOnce("bindTexture(ITexture3D) is a stub until Task 8 (cubemap)");
    }
    void bindTexture(rhi::ITexture2D* texture, unsigned int unit) override {
        BindTexture2D(texture, unit);
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
    // BlitMask 语义对照 VKRenderer::blitFramebuffer：
    // - Depth+dst==nullptr：src 离屏深度 CopyTextureRegion 拷入窗口深度（Defer lightbox）；
    // - Depth+dst!=nullptr：离屏 RT→RT 深度拷贝；
    // - Color：仅 dst!=nullptr 时执行 RT→RT 全屏三角形 blit（VK 不做 color→swapchain，
    //   对齐其行为）；MSAA src 的 resolve 场景由 Task 8 的 resolve 附件承担。
    // 资源经 IRenderTarget 接口取（colorTexture2D(0)/depthTexture2D()->handle() 返回
    // ID3D12Resource*，Task 8 的 DXRenderTarget 按此契约实现）；Task 7 期真实 RT 缺位
    // 时命中 WarnOnce 早退。
    void blitFramebuffer(const std::shared_ptr<IRenderTarget>& src,
                         const std::shared_ptr<IRenderTarget>& dst, BlitMask mask) override {
        if (!src || !_recording || !_cmdList.ptr) return;
        const bool doColor = (static_cast<uint8_t>(mask) & static_cast<uint8_t>(BlitMask::Color)) != 0;
        const bool doDepth = (static_cast<uint8_t>(mask) & static_cast<uint8_t>(BlitMask::Depth)) != 0;
        if (doDepth && !DoBlitDepth(src, dst)) return;
        if (doColor && dst) DoBlitColor(src, dst);
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
        // 上一样例写过的 SRV 槽全部置空描述符：旧样例纹理已销毁，
        // 悬垂 SRV 被新样例 draw 读到是未定义行为（对齐 VK 描述符集重建教训）
        if (_device.ptr && _srvHeap.ptr) {
            D3D12_SHADER_RESOURCE_VIEW_DESC nullSrv{};
            nullSrv.Format = DXGI_FORMAT_R8_UNORM;
            nullSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            nullSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            nullSrv.Texture2D.MostDetailedMip = 0;
            nullSrv.Texture2D.MipLevels = 1;
            for (unsigned unit : _boundUnits) {
                D3D12_CPU_DESCRIPTOR_HANDLE dst{_srvHeap->GetCPUDescriptorHandleForHeapStart()};
                dst.ptr += (unit + 1) * static_cast<SIZE_T>(_srvDescSize);
                _device->CreateShaderResourceView(nullptr, &nullSrv, dst);
            }
        }
        _boundUnits.clear();
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
        out.srvHeap = _srvHeap.ptr;   // Task 7：共享 SRV 堆（ImGui 字体纹理用槽 0）
        return _device.ptr && _queue.ptr;
    }

private:
    // ---- 内部 blit 能力（IDXBlitContext 实现，注入 DXTexture2D 做 mip 降采样）----
    // 嵌套类天然可访问外层私有成员；生命周期与 Renderer 一致
    class FrameBlitContext : public IDXBlitContext {
    public:
        explicit FrameBlitContext(DXRenderer* owner) : _owner(owner) {}
        ID3D12RootSignature* BlitRootSignature() override {
            return _owner->_blitRootSig.Get();
        }
        ID3D12PipelineState* BlitPsoFor(DXGI_FORMAT rtvFormat) override {
            return _owner->EnsureBlitPso(rtvFormat);
        }
    private:
        DXRenderer* _owner{nullptr};
    };

    // bindTexture 公共实现：SRV 写入共享 shader-visible 堆槽 unit+1
    // （槽 0 预留 ImGui，寄存器映射 t<slot>，与根签名 param1 表 t0..t127 一致）
    void BindTexture2D(rhi::ITexture2D* texture, unsigned int unit) {
        if (!texture || !_srvHeap.ptr) return;
        if (unit + 1 >= kSrvHeapSlots) {
            WarnOnce("bindTexture unit out of SRV heap range; ignored");
            return;
        }
        auto* dx = dynamic_cast<DXTexture2D*>(texture);
        if (!dx || !dx->valid()) return;
        if (dx->isMsaa()) {
            WarnOnce("bindTexture: MSAA textures are RTV-only; sampling requires resolve (Task 8)");
            return;
        }
        D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = dx->srvFormat();
        sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sd.Texture2D.MostDetailedMip = 0;
        sd.Texture2D.MipLevels = dx->mipLevels();
        D3D12_CPU_DESCRIPTOR_HANDLE dst{_srvHeap->GetCPUDescriptorHandleForHeapStart()};
        dst.ptr += (unit + 1) * static_cast<SIZE_T>(_srvDescSize);
        _device->CreateShaderResourceView(dx->resource(), &sd, dst);
        _boundUnits.insert(unit);
    }

    // blit PSO 缓存（键=目标 RTV 格式）：全屏三角形、无深度、无混合、无输入布局
    ID3D12PipelineState* EnsureBlitPso(DXGI_FORMAT rtvFormat) {
        auto it = _blitPsos.find(rtvFormat);
        if (it != _blitPsos.end()) return it->second.Get();
        ComPtr<ID3DBlob> vs = LoadBlobFromCso(
            DxSourcePath("_internal/blit.vert.hlsl").string(), ShaderStage::Vertex);
        ComPtr<ID3DBlob> ps = LoadBlobFromCso(
            DxSourcePath("_internal/blit.frag.hlsl").string(), ShaderStage::Fragment);
        if (!vs.Get() || !ps.Get()) {
            LOGE("[DX12] blit shaders not found under build/res/DX12/_internal");
            return nullptr;
        }
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = _blitRootSig.ptr;
        desc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        desc.SampleMask = UINT_MAX;
        // 其余状态零初始化即所需：Fill+无剔除、DepthEnable=FALSE、Blend 关闭
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = rtvFormat;
        desc.SampleDesc.Count = 1;
        ComPtr<ID3D12PipelineState> pso;
        DX_CHECK(_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)),
                 "create blit pipeline state");
        if (!pso.Get()) return nullptr;
        return _blitPsos.emplace(rtvFormat, std::move(pso)).first->second.Get();
    }

    // 懒建单槽暂存堆：颜色 blit 的临时 SRV（shader-visible）与 RTV
    bool EnsureScratchHeaps() {
        if (_scratchSrvHeap.ptr && _scratchRtvHeap.ptr) return true;
        D3D12_DESCRIPTOR_HEAP_DESC shd{};
        shd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        shd.NumDescriptors = 1;
        shd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        DX_CHECK(_device->CreateDescriptorHeap(&shd, IID_PPV_ARGS(&_scratchSrvHeap)),
                 "create blit scratch srv heap");
        D3D12_DESCRIPTOR_HEAP_DESC rhd{};
        rhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rhd.NumDescriptors = 1;
        DX_CHECK(_device->CreateDescriptorHeap(&rhd, IID_PPV_ARGS(&_scratchRtvHeap)),
                 "create blit scratch rtv heap");
        return _scratchSrvHeap.ptr && _scratchRtvHeap.ptr;
    }

    // 深度路径：src 深度 CopyTextureRegion 到 dst RT 或窗口深度（dst==nullptr）
    bool DoBlitDepth(const std::shared_ptr<IRenderTarget>& src,
                     const std::shared_ptr<IRenderTarget>& dst) {
        auto* srcDepthTex = dynamic_cast<DXTexture2D*>(src->depthTexture2D());
        ID3D12Resource* srcDepth = srcDepthTex ? srcDepthTex->resource() : nullptr;
        if (!srcDepth) {
            WarnOnce("blit depth: source has no depth texture yet (real RT lands in Task 8)");
            return false;
        }
        ID3D12Resource* dstDepth = nullptr;
        if (dst && dst->depthTexture2D()) {
            auto* dstDepthTex = dynamic_cast<DXTexture2D*>(dst->depthTexture2D());
            dstDepth = dstDepthTex ? dstDepthTex->resource() : nullptr;
        } else if (!dst && _swapchain) {
            dstDepth = _swapchain->depthResource();
        }
        if (!dstDepth) {
            WarnOnce("blit depth: destination has no depth target yet");
            return false;
        }
        // CopyTextureRegion 要求两端同格式（或同 TYPELESS family）：归一化为
        // typed 深度格式比较，不一致则拒绝（GL/VK blit 的跨格式转换 DX12 不提供）
        const DXGI_FORMAT srcFmt =
            DepthCopyFormat(srcDepth->GetDesc().Format);
        const DXGI_FORMAT dstFmt =
            DepthCopyFormat(dstDepth->GetDesc().Format);
        if (srcFmt != dstFmt) {
            LOGW("[DX12] blit depth: format mismatch {} vs {} rejected",
                 static_cast<int>(srcFmt), static_cast<int>(dstFmt));
            return false;
        }

        // 窗口深度当前若作为 DSV 绑定在 OM 上，状态切换前先解绑 DSV（保留颜色 RTV）；
        // GLBackend blit 后把 FBO 绑回默认帧缓冲 → 拷贝完成后恢复窗口 OM 绑定
        const bool windowBound = _rtActive && !_renderTarget && !_omPending;
        if (windowBound) {
            const uint32_t idx = _swapchain ? _swapchain->currentIndex() : 0;
            if (_swapchain && _swapchain->backBuffer(idx)) {
                auto rtvHandle = _swapchain->rtv(idx);
                _cmdList.ptr->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
            }
        }

        D3D12_RESOURCE_BARRIER toCopy[2] = {
            MakeTransition(srcDepth, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                           D3D12_RESOURCE_STATE_COPY_SOURCE),
            MakeTransition(dstDepth, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                           D3D12_RESOURCE_STATE_COPY_DEST),
        };
        _cmdList.ptr->ResourceBarrier(2, toCopy);

        // 全尺寸同格式拷贝（TYPELESS 资源两端同 family 合法）
        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource = dstDepth;
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource = srcDepth;
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex = 0;
        _cmdList.ptr->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        D3D12_RESOURCE_BARRIER toBack[2] = {
            MakeTransition(srcDepth, D3D12_RESOURCE_STATE_COPY_SOURCE,
                           D3D12_RESOURCE_STATE_DEPTH_WRITE),
            MakeTransition(dstDepth, D3D12_RESOURCE_STATE_COPY_DEST,
                           D3D12_RESOURCE_STATE_DEPTH_WRITE),
        };
        _cmdList.ptr->ResourceBarrier(2, toBack);

        // 对齐 GL 语义（blitFramebuffer 结尾 glBindFramebuffer(GL_FRAMEBUFFER, 0)）：
        // 深度拷入窗口后恢复窗口 OM 绑定（不清屏），后续 draw 直接可用
        if (!dst && windowBound) activateWindowTargets(false);
        return true;
    }

    // 颜色路径：全屏三角形把 src 颜色附件采样绘制到 dst 颜色附件（正高度视口，
    // 恒等方向对齐 VK vkCmdBlitImage 行为）
    void DoBlitColor(const std::shared_ptr<IRenderTarget>& src,
                     const std::shared_ptr<IRenderTarget>& dst) {
        auto* srcTex = dynamic_cast<DXTexture2D*>(src->colorTexture2D(0));
        auto* dstTex = dynamic_cast<DXTexture2D*>(dst->colorTexture2D(0));
        if (!srcTex || !dstTex || !srcTex->valid() || !dstTex->valid()) {
            WarnOnce("blit color: RT attachments unavailable until Task 8");
            return;
        }
        if (!EnsureScratchHeaps()) return;

        // src 建 SRV（常驻 PSHR 态可直接读）、dst 建临时 RTV
        D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = srcTex->srvFormat();
        sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sd.Texture2D.MostDetailedMip = 0;
        sd.Texture2D.MipLevels = srcTex->mipLevels();
        _device->CreateShaderResourceView(srcTex->resource(), &sd,
                                          _scratchSrvHeap->GetCPUDescriptorHandleForHeapStart());
        D3D12_RENDER_TARGET_VIEW_DESC rd{};
        rd.Format = dstTex->srvFormat();
        rd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rd.Texture2D.MipSlice = 0;
        _device->CreateRenderTargetView(dstTex->resource(), &rd,
                                        _scratchRtvHeap->GetCPUDescriptorHandleForHeapStart());

        ID3D12PipelineState* pso = EnsureBlitPso(dstTex->srvFormat());
        if (!pso) return;

        D3D12_RESOURCE_BARRIER toRT = MakeTransition(
            dstTex->resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        _cmdList.ptr->ResourceBarrier(1, &toRT);

        ID3D12DescriptorHeap* heaps[] = {_scratchSrvHeap.ptr};
        _cmdList.ptr->SetDescriptorHeaps(1, heaps);
        _cmdList.ptr->SetGraphicsRootSignature(_blitRootSig.ptr);
        _cmdList.ptr->SetPipelineState(pso);
        _cmdList.ptr->SetGraphicsRootDescriptorTable(
            0, _scratchSrvHeap->GetGPUDescriptorHandleForHeapStart());
        _cmdList.ptr->IASetVertexBuffers(0, 0, nullptr);
        D3D12_RESOURCE_DESC dd = dstTex->resource()->GetDesc();
        const float w = static_cast<float>(dd.Width);
        const float h = static_cast<float>(dd.Height);
        const D3D12_VIEWPORT vp{0.0f, 0.0f, w, h, 0.0f, 1.0f};
        const D3D12_RECT sc{0, 0, static_cast<LONG>(w), static_cast<LONG>(h)};
        _cmdList.ptr->RSSetViewports(1, &vp);
        _cmdList.ptr->RSSetScissorRects(1, &sc);
        auto rtvHandle = _scratchRtvHeap->GetCPUDescriptorHandleForHeapStart();
        _cmdList.ptr->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        _cmdList.ptr->DrawInstanced(3, 0, 0, 0);

        D3D12_RESOURCE_BARRIER toSRV = MakeTransition(
            dstTex->resource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        _cmdList.ptr->ResourceBarrier(1, &toSRV);
        applyViewport();   // 主路径负高度视口恢复，防后续 draw 沿用 blit 视口
    }

    static D3D12_RESOURCE_BARRIER MakeTransition(ID3D12Resource* res,
                                                 D3D12_RESOURCE_STATES before,
                                                 D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = res;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        return barrier;
    }

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

    // ---- 纹理/SRV（Task 7）----
    static constexpr unsigned kSrvHeapSlots = 128;   // 与根签名 param1 表 t0..t127 一致
    // 共享 shader-visible CBV_SRV_UAV 堆：槽 i ↔ 寄存器 t(i)，槽 0 预留 ImGui，
    // 纹理 bindTexture(unit) 写槽 unit+1（寄存器 t<unit+1>）
    ComPtr<ID3D12DescriptorHeap> _srvHeap;
    UINT _srvDescSize{0};
    std::set<unsigned> _boundUnits{};                // resetRenderState 时置空防悬垂描述符
    // 内部 blit（mip 降采样 + RT↔RT 颜色拷贝）：专用根签名 + 按目标格式缓存的 PSO
    ComPtr<ID3D12RootSignature> _blitRootSig;
    std::map<DXGI_FORMAT, ComPtr<ID3D12PipelineState>> _blitPsos{};
    FrameBlitContext _blitCtx;
    ComPtr<ID3D12DescriptorHeap> _scratchSrvHeap;    // 颜色 blit 单槽暂存 SRV/RTV
    ComPtr<ID3D12DescriptorHeap> _scratchRtvHeap;

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
    if (!CreateBlitRootSignature(_device.ptr, &_blitRootSig)) return false;

    // 共享 SRV 堆（shader-visible）：bindTexture 写槽 unit+1，draw 前 SetDescriptorHeaps
    D3D12_DESCRIPTOR_HEAP_DESC shd{};
    shd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    shd.NumDescriptors = kSrvHeapSlots;
    shd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    DX_CHECK(_device->CreateDescriptorHeap(&shd, IID_PPV_ARGS(&_srvHeap)), "create srv heap");
    if (!_srvHeap.ptr) return false;
    _srvDescSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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
    _blitPsos.clear();
    if (_blitRootSig.Get()) { _blitRootSig->Release(); _blitRootSig.ptr = nullptr; }
    if (_scratchRtvHeap.Get()) { _scratchRtvHeap->Release(); _scratchRtvHeap.ptr = nullptr; }
    if (_scratchSrvHeap.Get()) { _scratchSrvHeap->Release(); _scratchSrvHeap.ptr = nullptr; }
    if (_srvHeap.Get()) { _srvHeap->Release(); _srvHeap.ptr = nullptr; }
    _boundUnits.clear();
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
    // SRV 描述符堆 + 根表（param1，t0..t127 ↔ 堆槽 0..127）：静态采样器已烘焙进
    // 根签名随 SetGraphicsRootSignature 生效，无需每 draw 额外采样器绑定。
    // 每次 draw 重设堆：mipgen/blit 可能中途切换过堆，此处自愈
    if (_srvHeap.ptr) {
        ID3D12DescriptorHeap* heaps[] = {_srvHeap.ptr};
        _cmdList.ptr->SetDescriptorHeaps(1, heaps);
        _cmdList.ptr->SetGraphicsRootDescriptorTable(
            1, _srvHeap->GetGPUDescriptorHandleForHeapStart());
    }

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
