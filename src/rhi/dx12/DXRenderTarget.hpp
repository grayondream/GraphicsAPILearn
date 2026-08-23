#pragma once
#include "rhi/dx12/DXHeader.hpp"
#include "rhi/dx12/DXTexture2D.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include <array>
#include <memory>
#include <vector>

namespace rhi {

class DXTexture3D;

// 离屏渲染目标（Task 8，对照 VKRenderTarget 接口面）：
// - 附件：颜色/深度按 FramebufferDesc 内部建 DXTexture2D（非 MSAA 颜色可采样、
//   MSAA 颜色 RTV-only DENY_SHADER_RESOURCE、深度 TYPELESS+ALLOW_DEPTH_STENCIL）；
//   colorTexture2D(i)/depthTexture2D() 直接返回内部纹理（handle()==ID3D12Resource*，
//   满足 Task 7 blitFramebuffer 资源契约）；MSAA resolve 经 blitFramebuffer 的
//   ResolveSubresource 路径承担（resolveTo 与 VK 一致为 no-op）；
// - 描述符：自持 RTV 堆（owned 颜色槽位）与 DSV 堆（内部深度）；attachCubeFace/
//   attachDepthCube 的面句柄取自 DXTexture3D 自持堆，本类不复制描述符；
// - 状态机（Renderer flushOmTargets 编排）：BeginPass 把附件从常驻可采样态屏障到
//   渲染态并 OMSetRenderTargets 前置就绪；EndPass 反向恢复——颜色 RENDER_TARGET→
//   PSHR、深度 DEPTH_WRITE→PSHR（对齐 VK finalLayout=eShaderReadOnlyOptimal，
//   保证 pass 间采样/深度调试 quad 可用）。MSAA 颜色常驻 RENDER_TARGET 不参与转移。
class DXRenderTarget : public IRenderTarget {
public:
    // 与 DXTexture2D 相同的后端句柄组（共享 upload allocator + fence 单调推进）
    DXRenderTarget(ID3D12Device* device, ID3D12CommandQueue* queue,
                   ID3D12CommandAllocator* uploadAlloc, ID3D12Fence* uploadFence,
                   HANDLE fenceEvent);
    ~DXRenderTarget() override;

    bool create(int width, int height) override;                          // 旧签名：RGBA8 + Depth24Stencil8
    bool create(const FramebufferDesc& desc) override;
    bool attachCubeFace(ITexture3D* cube, int face, int mip = 0) override;
    bool attachDepthCube(ITexture3D* cube, int mip = 0) override;
    bool bind() override { return true; }                                 // 绑定由 Renderer 统一编排
    bool unbind() override { return true; }
    void* colorTexture() override;
    ITexture2D* colorTexture2D(int attachment = 0) override;
    ITexture2D* depthTexture2D() override;
    bool resolveTo(IRenderTarget& dst) override;                          // MSAA resolve 走 blitFramebuffer
    void* handle() override;                                              // 颜色 0 资源（无颜色返回 nullptr）
    void release() override;

    // ---- Renderer 协作 ----
    bool valid() const { return _valid; }
    uint32_t colorCount() const;                                          // OM 颜色目标数（cube-color attach 时=1）
    DXGI_FORMAT colorFormat(uint32_t i) const;                            // PSOKey 用
    DXGI_FORMAT depthFormat() const;                                      // 无深度=UNKNOWN
    uint32_t sampleCount() const { return _samples; }
    bool hasDepthAttachment() const;                                      // 内部深度或已挂深度 cube
    void renderDims(int& w, int& h) const;                                // viewport 兜底尺寸（cube 面=mip 尺寸）

    // pass 编排：BeginPass 幂等（已在位则不重复屏障）；EndPass 恢复常驻态
    void BeginPass(ID3D12GraphicsCommandList* cmd);
    void EndPass(ID3D12GraphicsCommandList* cmd);
    // OM 句柄组：MRT 数组路径（Defer/SSAO GBuffer 多 RTV），深度句柄可选
    void GetOmTargets(std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& rtvs,
                      D3D12_CPU_DESCRIPTOR_HANDLE& dsv, bool& hasDsv) const;
    // 当前深度资源状态（blit 状态感知转移用；无深度时返回 DEPTH_WRITE 占位）
    D3D12_RESOURCE_STATES currentDepthState() const {
        return _depthInWrite ? D3D12_RESOURCE_STATE_DEPTH_WRITE
                             : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
    // 清屏：颜色 RTV ×N + 深度/模板 DSV（stencil 标志按格式有无裁剪）
    void ClearAll(ID3D12GraphicsCommandList* cmd, const std::array<float, 4>& cc);

private:
    ID3D12Device* _device{nullptr};
    ID3D12CommandQueue* _queue{nullptr};
    ID3D12CommandAllocator* _uploadAlloc{nullptr};
    ID3D12Fence* _uploadFence{nullptr};
    HANDLE _fenceEvent{nullptr};

    int _width{0};
    int _height{0};
    UINT _samples{1};

    std::vector<std::shared_ptr<DXTexture2D>> _colors{};     // 非 MSAA 颜色（PSHR↔RT 往返）
    std::vector<std::shared_ptr<DXTexture2D>> _msaaColors{}; // MSAA 颜色（常驻 RENDER_TARGET）
    std::shared_ptr<DXTexture2D> _depth{};                   // 内部深度（可空）

    ComPtr<ID3D12DescriptorHeap> _rtvHeap;                   // 槽序=[_colors..., _msaaColors...]
    ComPtr<ID3D12DescriptorHeap> _dsvHeap;                   // 单槽：内部深度
    DXGI_FORMAT _depthStencilFormat{DXGI_FORMAT_UNKNOWN};    // 内部深度 DSV/PSO typed 格式

    // cube 挂接状态（弱引用，App 持有纹理生命周期；face<0 表示仅 attachDepthCube 未逐面挂接）
    DXTexture3D* _cube{nullptr};
    int _face{-1};
    int _mip{0};
    bool _cubeIsDepth{false};

    // BeginPass 生效时的挂接快照：逐面捕获循环里 attachCubeFace 在 EndPass 之前
    // 就改写 _face/_mip，EndPass 必须按"本 pass 实际渲染"的面恢复常驻态——
    // 读当前挂接值会对新面发无效屏障且旧面滞留 RENDER_TARGET（终审 F2）
    DXTexture3D* _boundCube{nullptr};
    int _boundFace{-1};
    int _boundMip{0};
    bool _boundCubeIsDepth{false};

    // 状态跟踪（BeginPass/EndPass 幂等依据；blit 状态感知转移读取）
    bool _colorsInRT{false};        // owned 非 MSAA 颜色当前处于 RENDER_TARGET
    bool _depthInWrite{true};       // 当前深度资源处于 DEPTH_WRITE
    bool _cubeFaceInRT{false};      // 已挂颜色 cube 面子资源当前处于 RENDER_TARGET
    bool _valid{false};
};

} // namespace rhi
