#pragma once
#include "rhi/dx12/DXHeader.hpp"
#include "rhi/dx12/DXTexture2D.hpp"
#include "rhi/core/ITexture3D.hpp"
#include <functional>

namespace rhi {

// Cubemap / 3D 纹理（Task 8，对照 VKTexture3D 接口面）：
// - 存储：cubemap=Texture2D 数组（DepthOrArraySize=6，D3D12 无需 cube 兼容标志，
//   TEXTURECUBE SRV 视图即可采样）；init() 走真正的 Texture3D 维度（RGBA8）。
//   深度格式按 TYPELESS 家族建资源（Depth32F→R32_TYPELESS；D24S8 直接 typed），
//   DSV/SRV 视图阶段取 typed 格式（同 DXTexture2D::createEmpty 约定）；
// - 上传：DEFAULT 堆资源 + UPLOAD 暂存逐面 CopyTextureRegion（子资源索引 =
//   mip + face*mipLevels），fence 同步（同 DXBuffer/DXTexture2D 模式）；
// - mipmap：generateMipmap=true 分配标准链长，genCubeMipmaps 用 blit PSO 逐面
//   逐级线性降采样（子资源级 PSHR↔RENDER_TARGET 往返），blit 能力经
//   IDXBlitContext 注入（与 DXTexture2D mipgen 同一套 RS+PSO）；
// - 渲染目标接入：颜色 cube 提供 rtvFace(face,mip)（独立 RTV 堆）、深度 cube
//   提供 dsvFace(face)（独立 DSV 堆），DXRenderTarget::attachCubeFace 取句柄
//   组 OM 目标；子资源状态转移由 Renderer/RT 编排（本类不持状态机）。
class DXTexture3D : public ITexture3D {
public:
    // 与 DXTexture2D 相同的后端句柄组（共享 upload allocator + fence 单调推进）
    DXTexture3D(ID3D12Device* device, ID3D12CommandQueue* queue,
                ID3D12CommandAllocator* uploadAlloc, ID3D12Fence* uploadFence,
                HANDLE fenceEvent);
    ~DXTexture3D() override;

    bool init(const TextureDataView3D& data) override;                  // 旧签名：RGBA8 3D 纹理
    bool initCube(const TextureDesc& desc, const TextureDataView2D* faces) override;
    bool createEmpty(const TextureDesc& desc, int width, int height) override;
    void bind(unsigned int unit = 0) override;                          // 绑定=Renderer 写 SRV 堆槽
    void* handle() override;
    bool valid() const override { return _valid; }
    void release() override;
    void genCubeMipmaps() override;

    // ---- Renderer / RT 访问器 ----
    void setBlitContext(IDXBlitContext* ctx) { _blitCtx = ctx; }        // init 前注入
    ID3D12Resource* resource() const { return _resource.Get(); }
    DXGI_FORMAT storageFormat() const { return _format; }               // 资源存储格式（深度 TYPELESS）
    DXGI_FORMAT srvFormat() const { return _srvFormat; }                // SRV 视图 typed 格式
    DXGI_FORMAT rtvFormat() const { return _rtvFormat; }                // 颜色 cube RTV typed 格式
    DXGI_FORMAT dsvFormat() const { return _dsvFormat; }                // 深度 cube DSV typed 格式
    UINT mipLevels() const { return _mipLevels; }
    int width() const { return _width; }
    int height() const { return _height; }
    bool isCube() const { return _cube; }
    bool isDepth() const { return _depth; }

    // face+mip 子资源索引（Texture2D 数组布局：mip + face * mipLevels）
    UINT subresource(int face, int mip) const {
        return static_cast<UINT>(mip) + static_cast<UINT>(face) * _mipLevels;
    }

    // 颜色 cube 面 RTV / 深度 cube 面 DSV：各自独立非 shader-visible 堆，
    // 首次访问时创建并一次性填充全部描述符（attachCubeFace 渲染路径取用）
    D3D12_CPU_DESCRIPTOR_HANDLE rtvFace(int face, int mip);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvFace(int face);

    // 写 cubemap/3D SRV 到指定 CPU 描述符（bindTexture 共享堆槽路径用）：
    // cube→TEXTURECUBE 全 mip 链，深度取 typed 格式；3D→TEXTURE3D
    bool WriteSrv(D3D12_CPU_DESCRIPTOR_HANDLE dst);

    // 采样语义（bind 路径决定动态采样器堆槽位）
    const TextureDesc& samplerParams() const { return _params; }

private:
    bool createCubeStorage(int width, int height, UINT mips, D3D12_RESOURCE_FLAGS flags);
    bool uploadFaces(const TextureDataView2D* faces);
    bool recordCubeMipgen(ID3D12GraphicsCommandList* cmd,
                          ID3D12DescriptorHeap* srvHeap, ID3D12DescriptorHeap* rtvHeap);
    bool executeOneShot(const std::function<void(ID3D12GraphicsCommandList*)>& record);

    ID3D12Device* _device{nullptr};
    ID3D12CommandQueue* _queue{nullptr};
    ID3D12CommandAllocator* _uploadAlloc{nullptr};
    ID3D12Fence* _uploadFence{nullptr};
    HANDLE _fenceEvent{nullptr};

    IDXBlitContext* _blitCtx{nullptr};   // 弱引用（Renderer 注入，mipgen 用）
    ComPtr<ID3D12Resource> _resource{};
    DXGI_FORMAT _format{DXGI_FORMAT_UNKNOWN};     // 资源存储格式
    DXGI_FORMAT _srvFormat{DXGI_FORMAT_UNKNOWN};  // SRV 视图 typed 格式
    DXGI_FORMAT _rtvFormat{DXGI_FORMAT_UNKNOWN};  // 颜色 RTV 视图格式
    DXGI_FORMAT _dsvFormat{DXGI_FORMAT_UNKNOWN};  // 深度 DSV 视图格式
    UINT _mipLevels{1};
    int _width{0};
    int _height{0};
    int _depthSlices{0};                 // init(3D) 的 z 维度（cube 恒 6）
    bool _cube{false};
    bool _depth{false};
    bool _valid{false};

    // 面 RTV / 面 DSV 描述符堆（懒建于首次访问，release 时释放）
    ComPtr<ID3D12DescriptorHeap> _faceRtvHeap;
    ComPtr<ID3D12DescriptorHeap> _faceDsvHeap;

    TextureDesc _params{};               // 创建时的采样语义（filter/wrap）
};

} // namespace rhi
