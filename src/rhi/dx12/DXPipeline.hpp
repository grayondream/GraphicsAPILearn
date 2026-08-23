#pragma once
#include "rhi/dx12/DXHeader.hpp"
#include "rhi/core/IPipeline.hpp"

namespace rhi {

class DXShader;

// PSO 缓存键：状态哈希 + 渲染目标格式布局。D3D12 的混合/深度/模板/光栅化状态全部
// 烘焙进 PSO（无 VK 式动态状态），状态一变即换键，缓存天然按值区分、无需失效标记
// （对照 VKPipeline：VK 把可动态的状态留给 applyDynamicState，仅 blend/polygon/
// multisample/topology 参与 _needsRecreate——DX 同构为"全集进哈希"）。
struct PSOKey {
    uint32_t stateHash{0};
    DXGI_FORMAT color[8]{};   // 最多 8 个颜色附件（MRT）
    uint32_t colorCount{0};
    DXGI_FORMAT depth{DXGI_FORMAT_UNKNOWN};  // 无深度目标时 UNKNOWN
    uint32_t samples{1};

    bool operator==(const PSOKey& o) const;
};

struct PSOKeyHash {
    size_t operator()(const PSOKey& k) const;
};

// TextureFilter(3) × TextureWrap(3) 全组合 → 采样器槽位（0..8）
int SamplerSlot(TextureFilter filter, TextureWrap wrap);
// 静态采样器表与条目数（挂在 root signature 尾部）。ClampToBorder 组合不在静态表：
// 静态采样器只有黑/白边框色，任意 borderColor 由 DXRenderer 的动态 SAMPLER 堆
// （槽位沿用 f*3+w 编号）在 bind 路径写入。
const D3D12_STATIC_SAMPLER_DESC* StaticSamplers(size_t& count);
// 动态采样器堆构建辅助（DXBackend 用）：filter/wrap → D3D12 描述
D3D12_FILTER DxFilterOf(TextureFilter filter);
D3D12_TEXTURE_ADDRESS_MODE DxAddressOf(TextureWrap wrap);
// 全局单例 root signature：param0 = 根 CBV(b0, ALL)，param1 = 表 SRV t0..127(PIXEL)
bool CreateSharedRootSignature(ID3D12Device* device, ComPtr<ID3D12RootSignature>& out);

class DXPipeline : public IPipeline {
public:
    // rootSig 归 DXRenderer 所有（全局单例），此处仅借用
    DXPipeline(ID3D12Device* device, ID3D12RootSignature* rootSig,
               const VertexLayout& layoutIn, std::shared_ptr<IShader> shader);
    ~DXPipeline() override = default;

    void use() override {}   // GL use() 绑 program+VAO；DX 状态在 draw 时由 Renderer 装配
    void* handle() override; // 最近一次构建的 PSO（无则 nullptr）

    // 与 VK 一致：uniform 走显式 UBO，pipeline 侧 no-op 返回 false
    bool setUniform(const std::string&, bool) override { return false; }
    bool setUniform(const std::string&, int) override { return false; }
    bool setUniform(const std::string&, float) override { return false; }
    bool setUniform(const std::string&, const float*, int) override { return false; }
    bool setUniform(const std::string&, const float*, int, int) override { return false; }
    bool setUniformMatrix(const std::string&, const float*, int, int) override { return false; }
    void bindUniformBlock(uint32_t) override {}  // b0 已硬编码进 root signature

    // 渲染状态 setter：只写成员，哈希按需重算——不同状态自然落不同缓存键
    void setDepthTest(bool enable) override { _depthTest = enable; }
    void setCullMode(bool enable, int face) override { _cullEnable = enable; _cullFace = static_cast<CullFace>(face); }
    void setBlend(bool enable) override { _blend = enable; }
    void setDepthFunc(CompareFunc func) override { _depthFunc = func; }
    void setDepthMask(bool write) override { _depthMask = write; }
    void setStencilTest(bool enable) override { _stencilTest = enable; }
    void setStencilFunc(CompareFunc func, int ref, unsigned mask) override { _stencilFunc = func; _stencilRef = ref; _stencilCompareMask = mask; }
    void setStencilOp(StencilOp sfail, StencilOp dpfail, StencilOp dppass) override { _stencilFail = sfail; _stencilDepthFail = dpfail; _stencilPass = dppass; }
    void setStencilMask(unsigned mask) override { _stencilWriteMask = mask; }
    void setBlendFunc(BlendFactor src, BlendFactor dst) override { _blendSrc = src; _blendDst = dst; }
    void setCullFaceEnable(bool enable) override { _cullEnable = enable; }
    void setCullFace(CullFace face) override { _cullFace = face; }
    void setFrontFace(bool ccw) override { _frontFaceCCW = ccw; }
    void setPolygonMode(PolygonMode mode) override { _polygonMode = mode; }
    void setPointSizeProgramEnable(bool enable) override { _pointSizeEnable = enable; }  // GL 专属开关，不参与 PSO
    void setMultisample(bool enable) override { _multisample = enable; }
    void setPrimitiveType(PrimitiveType type) override { _primitive = type; }
    PrimitiveType primitiveType() const override { return _primitive; }

    // Renderer 绑定顶点缓冲时按 binding 取 stride（IBuffer 无法自述步长）
    const VertexLayout& layout() const { return _layout; }

    // D3D12 模板参考值是命令流动态状态（OMSetStencilRef，同 VK 动态下发），
    // 不参与 PSO——Renderer 每 draw 取用
    int stencilRef() const { return _stencilRef; }

    // 缓冲绑定发生在 draw 时（Renderer 持 layout stride 调 BindAsVB，同 VK）
    void setVertexBuffer(const std::shared_ptr<IBuffer>&, uint32_t) override {}
    void setIndexBuffer(const std::shared_ptr<IBuffer>&) override {}

    uint32_t stateHash() const;
    // Renderer::setPipeline→flush 时调用：未命中则按 key 构建 PSO 并入缓存。
    // colorCount/samples 必须与当前渲染目标一致（同 VK pipelineFor(rp, samples)）。
    ID3D12PipelineState* pipelineFor(const PSOKey& key);

private:
    bool createGraphicsPipeline(const PSOKey& key, ComPtr<ID3D12PipelineState>& out);

    ID3D12Device* _device{nullptr};
    ID3D12RootSignature* _rootSignature{nullptr};
    VertexLayout _layout{};
    std::shared_ptr<DXShader> _shader{};
    std::unordered_map<PSOKey, ComPtr<ID3D12PipelineState>, PSOKeyHash> _cache{};
    ID3D12PipelineState* _last{nullptr};

    // 状态全集镜像 VKPipeline.hpp 成员与默认值
    bool _depthTest{false};
    bool _depthMask{true};
    CompareFunc _depthFunc{CompareFunc::Less};
    bool _cullEnable{false};
    CullFace _cullFace{CullFace::Back};
    bool _frontFaceCCW{true};
    bool _stencilTest{false};
    CompareFunc _stencilFunc{CompareFunc::Always};
    int _stencilRef{0};
    unsigned _stencilCompareMask{0xFF};
    StencilOp _stencilFail{StencilOp::Keep};
    StencilOp _stencilDepthFail{StencilOp::Keep};
    StencilOp _stencilPass{StencilOp::Keep};
    unsigned _stencilWriteMask{0xFF};
    bool _blend{false};
    BlendFactor _blendSrc{BlendFactor::SrcAlpha};
    BlendFactor _blendDst{BlendFactor::OneMinusSrcAlpha};
    PolygonMode _polygonMode{PolygonMode::Fill};
    bool _pointSizeEnable{false};
    bool _multisample{false};
    PrimitiveType _primitive{PrimitiveType::TriangleList};
};

} // namespace rhi
