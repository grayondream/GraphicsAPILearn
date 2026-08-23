#pragma once
#include "rhi/dx12/DXHeader.hpp"
#include "rhi/core/ITexture2D.hpp"
#include <functional>
#include <array>

namespace rhi {

// 内部 blit 能力借用接口（DXRenderer 嵌套类实现）：mipmap 逐级降采样需要
// 全屏三角形 PSO 与专用 root signature，二者归 Renderer 所有、Texture 弱引用。
// 生命周期约定：setBlitContext 注入的指针须在纹理 init() 期间保持有效
// （Renderer 先于全部 App 资源创建/销毁）。
class IDXBlitContext {
public:
    virtual ~IDXBlitContext() = default;
    virtual ID3D12RootSignature* BlitRootSignature() = 0;
    // 按 RTV 格式取/建全屏三角形 blit PSO（无深度/无混合/无输入布局）
    virtual ID3D12PipelineState* BlitPsoFor(DXGI_FORMAT rtvFormat) = 0;
    // 数组变体：源 SRV 为 TEXTURE2DARRAY 视图（cubemap 逐面逐级 mipgen 用），
    // 与 Texture2D 声明混用属视图类型不匹配 UB，须用 blit_array.frag 的 PSO
    virtual ID3D12PipelineState* BlitArrayPsoFor(DXGI_FORMAT rtvFormat) = 0;
};

// 2D 纹理（Task 7）：
// - 上传：DEFAULT 堆资源 + UPLOAD 暂存 CopyTextureRegion（fence 同步，同 DXBuffer 模式）；
//   RGB8 / RGB16F / RGBA32F 等 3 通道源数据 CPU 展开为 4 分量存储布局
//   （对齐 VKTexture2D 依赖 RhiImage 预展开 + 本端兜底）；
// - mipmap：D3D12 无 GenerateMips 内建 API，用 blit PSO 渲到各 mip 层 RTV 手动线性
//   降采样（子资源级状态切换 PSHR↔RENDER_TARGET），blit 能力经 IDXBlitContext 注入；
// - MSAA（multisample=true）：按 samples 建 4X/8X 资源，仅 ALLOW_RENDER_TARGET
//   （DENY_SHADER_RESOURCE 与其组合非法；RTV-only 由 isMsaa 守卫保证）；
//   采样数经 CheckFeatureSupport 校验回落。
// - 状态约定：非 MSAA 颜色常驻 PIXEL_SHADER_RESOURCE（bindTexture 的 SRV 直接可用），
//   深度常驻 DEPTH_WRITE；MSAA 仅作 RTV 由 Task 8 渲染路径管理。
class DXTexture2D : public ITexture2D {
public:
    // 与 DXBuffer 相同的后端句柄组（共享 upload allocator + fence 单调推进）
    DXTexture2D(ID3D12Device* device, ID3D12CommandQueue* queue,
                ID3D12CommandAllocator* uploadAlloc, ID3D12Fence* uploadFence,
                HANDLE fenceEvent);
    ~DXTexture2D() override;

    bool init(const TextureDataView2D& data) override;                  // 旧签名：默认 RGBA8 desc
    bool init(const TextureDesc& desc, const TextureDataView2D& data) override;
    bool createEmpty(const TextureDesc& desc, int width, int height) override;
    void bind(unsigned int unit = 0) override;                          // 绑定=Renderer 写 SRV 堆槽
    void* handle() override;
    bool valid() const override { return _valid; }
    void release() override;

    // ---- Renderer 访问器 ----
    void setBlitContext(IDXBlitContext* ctx) { _blitCtx = ctx; }        // init 前注入
    ID3D12Resource* resource() const { return _resource.Get(); }
    DXGI_FORMAT storageFormat() const { return _format; }               // 资源存储格式（深度为 TYPELESS）
    DXGI_FORMAT srvFormat() const { return _srvFormat; }                // SRV/RTV 视图 typed 格式
    UINT mipLevels() const { return _mipLevels; }
    bool isMsaa() const { return _msaa; }                               // RTV-only，不可建 SRV
    // 采样语义（bind 路径决定是否需要动态采样器堆槽位；RT 内部纹理由
    // DXRenderTarget 按 FramebufferAttachment 回填）
    const TextureDesc& samplerParams() const { return _params; }
    const std::array<float, 4>& borderColor() const { return _borderColor; }
    void setBorderColor(const float bc[4]);

private:
    bool createResource(UINT width, UINT height, D3D12_RESOURCE_FLAGS flags, UINT sampleCount);
    bool uploadAndGenMips(const TextureDesc& desc, const TextureDataView2D& data);
    bool CreateMipgenHeaps(ComPtr<ID3D12DescriptorHeap>& srvOut, ComPtr<ID3D12DescriptorHeap>& rtvOut);
    bool recordMipgen(ID3D12GraphicsCommandList* cmd,
                      ID3D12DescriptorHeap* srvHeap, ID3D12DescriptorHeap* rtvHeap);
    bool executeOneShot(const std::function<void(ID3D12GraphicsCommandList*)>& record);

    ID3D12Device* _device{nullptr};
    ID3D12CommandQueue* _queue{nullptr};
    ID3D12CommandAllocator* _uploadAlloc{nullptr};
    ID3D12Fence* _uploadFence{nullptr};
    HANDLE _fenceEvent{nullptr};

    IDXBlitContext* _blitCtx{nullptr};   // 弱引用（Renderer 注入）
    ComPtr<ID3D12Resource> _resource{};
    DXGI_FORMAT _format{DXGI_FORMAT_UNKNOWN};    // 资源存储格式（深度为 TYPELESS）
    DXGI_FORMAT _srvFormat{DXGI_FORMAT_UNKNOWN}; // SRV/RTV 视图 typed 格式
    UINT _mipLevels{1};
    int _width{0};
    int _height{0};
    bool _msaa{false};
    bool _valid{false};

    TextureDesc _params{};                      // 创建时的采样语义（filter/wrap）
    std::array<float, 4> _borderColor{{1.0f, 1.0f, 1.0f, 1.0f}};  // ClampToBorder 边框色
};

} // namespace rhi
