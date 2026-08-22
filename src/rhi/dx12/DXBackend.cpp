#include "rhi/dx12/DXBackend.hpp"
#include "rhi/dx12/DXBuffer.hpp"
#include "rhi/core/ISurface.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/ITexture3D.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ISwapchain.hpp"

namespace rhi {

namespace {

// Task 2 仅做生命周期空壳：工厂返回 no-op 对象而非 nullptr，
// 使样例在 createShader().compile()==false 处经 ExitIfFailed 干净退出，
// 避免 App 层对空指针解引用段错误。后续任务以同名实现类逐个替换。
class DXNullShader : public IShader {
public:
    bool compile(const std::vector<ShaderStage>&) override { return false; }
    std::string getLog() const override { return "DX12 shader not implemented yet (Task 4)"; }
    bool valid() const override { return false; }
};

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

class DXNullSwapchain : public ISwapchain {
public:
    bool present() override { return false; }
    void resize(int, int) override {}
    void* handle() override { return nullptr; }
};

} // namespace

class DXRenderer : public IRenderer {
public:
    ~DXRenderer() override { shutdown(); }

    bool init(const std::shared_ptr<ISurface>& surface) override;
    void shutdown() override;

    std::shared_ptr<IShader> createShader() override { return std::make_shared<DXNullShader>(); }
    std::shared_ptr<IPipeline> createPipeline(const VertexLayout&, const std::shared_ptr<IShader>&) override { return std::make_shared<DXNullPipeline>(); }
    std::shared_ptr<IBuffer> createBuffer() override {
        if (!_device.ptr) { LOGE("[DX12] createBuffer before init"); return std::make_shared<DXNullBuffer>(); }
        return std::make_shared<DXBuffer>(_device.ptr, _queue.ptr, _uploadAllocator.ptr, _frameFence.ptr, _fenceEvent);
    }
    std::shared_ptr<IBuffer> createUniformBuffer() override {
        if (!_device.ptr) { LOGE("[DX12] createUniformBuffer before init"); return std::make_shared<DXNullBuffer>(); }
        return std::make_shared<DXBuffer>(_device.ptr, _queue.ptr, _uploadAllocator.ptr, _frameFence.ptr, _fenceEvent);
    }
    std::shared_ptr<ITexture2D> createTexture2D() override { return std::make_shared<DXNullTexture2D>(); }
    std::shared_ptr<ITexture3D> createTexture3D() override { return std::make_shared<DXNullTexture3D>(); }
    std::shared_ptr<IRenderTarget> createRenderTarget() override { return std::make_shared<DXNullRenderTarget>(); }
    std::shared_ptr<ISwapchain> getSwapchain() override { return std::make_shared<DXNullSwapchain>(); }

    void beginFrame() override {}
    void endFrame() override {}
    bool present() override { return false; }

    void clearColor(float, float, float, float) override {}
    void setViewport(const Viewport& vp) override { _viewport = vp; }
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
    BackendCapabilities backendCapabilities() override { return {}; }

private:
    std::shared_ptr<ISurface> _surface;
    Viewport _viewport{};
    ComPtr<ID3D12Device> _device;
    ComPtr<ID3D12CommandQueue> _queue;
    ComPtr<ID3D12Fence> _frameFence;
    HANDLE _fenceEvent{nullptr};
    // 缓冲初始化数据的一次性拷贝命令与 VB/IB 上传共用此 direct allocator（串行使用，用后 Reset）
    ComPtr<ID3D12CommandAllocator> _uploadAllocator;
};

bool DXRenderer::init(const std::shared_ptr<ISurface>& surface) {
    _surface = surface;
    // IID_PPV_ARGS 宏展开为逗号分隔的两个实参，不能放进三目表达式（brief 原式编译不过）；
    // 调试层启用决策延后到后续任务，这里仅探测可用性。
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
    LOGI("[DX12] device ready");
    return true;
}

void DXRenderer::shutdown() {
    if (_fenceEvent) { CloseHandle(_fenceEvent); _fenceEvent = nullptr; }
    if (_uploadAllocator.Get()) { _uploadAllocator->Release(); _uploadAllocator.ptr = nullptr; }
    if (_frameFence.Get()) { _frameFence->Release(); _frameFence.ptr = nullptr; }
    if (_queue.Get()) { _queue->Release(); _queue.ptr = nullptr; }
    if (_device.Get()) { _device->Release(); _device.ptr = nullptr; }
}

std::shared_ptr<IRenderer> createDX12Renderer() { return std::make_shared<DXRenderer>(); }

} // namespace rhi
