#include "rhi/dx12/DXPipeline.hpp"
#include "rhi/dx12/DXShader.hpp"
#include "base/Log.hpp"
#include <array>
#include <vector>

namespace rhi {

namespace {

// 注意：DxFilterOf/DxAddressOf 的定义在下方 rhi 命名空间作用域（与 DXPipeline.hpp
// 声明配对，DXBackend 动态采样器堆复用）。不得再于匿名命名空间定义同签名函数——
// 两份可见重载会让 MSVC 报调用二义（C2668）。

D3D12_STATIC_SAMPLER_DESC MakeSampler(D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE address, UINT slot,
                                      D3D12_COMPARISON_FUNC comparison = D3D12_COMPARISON_FUNC_NEVER) {
    D3D12_STATIC_SAMPLER_DESC s{};
    s.Filter = filter;
    s.AddressU = address;
    s.AddressV = address;
    s.AddressW = address;
    s.MipLODBias = 0;
    s.MaxAnisotropy = 1;   // 非各向异性过滤时必须为 1
    // 非 comparison 采样器该字段无效果；NONE 常量需较新 SDK，用语义等价的 NEVER 兼容旧头
    s.ComparisonFunc = comparison;
    // 静态采样器只有黑/白两种边框色：对齐 VKTexture2D adopt 的 OPAQUE_WHITE 选择
    // （阴影贴图越界=远处=受光）；逐纹理 borderColor 动态需求延后 Task 8 采样器堆
    s.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    s.MinLOD = 0;
    s.MaxLOD = D3D12_FLOAT32_MAX;
    s.ShaderRegister = slot;
    s.RegisterSpace = 0;
    s.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    return s;
}

DXGI_FORMAT ToDxVertexFormat(VertexElement::Format format) {
    switch (format) {
        case VertexElement::Float2: return DXGI_FORMAT_R32G32_FLOAT;
        case VertexElement::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
        case VertexElement::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case VertexElement::Int4:   return DXGI_FORMAT_R32G32B32A32_SINT;
    }
    return DXGI_FORMAT_R32G32B32_FLOAT;
}

D3D12_COMPARISON_FUNC ToDxCompare(CompareFunc func) {
    switch (func) {
        case CompareFunc::Never:        return D3D12_COMPARISON_FUNC_NEVER;
        case CompareFunc::Less:         return D3D12_COMPARISON_FUNC_LESS;
        case CompareFunc::Equal:        return D3D12_COMPARISON_FUNC_EQUAL;
        case CompareFunc::LessEqual:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case CompareFunc::Greater:      return D3D12_COMPARISON_FUNC_GREATER;
        case CompareFunc::NotEqual:     return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case CompareFunc::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case CompareFunc::Always:       return D3D12_COMPARISON_FUNC_ALWAYS;
    }
    return D3D12_COMPARISON_FUNC_ALWAYS;
}

D3D12_STENCIL_OP ToDxStencilOp(StencilOp op) {
    switch (op) {
        case StencilOp::Keep:     return D3D12_STENCIL_OP_KEEP;
        case StencilOp::Zero:     return D3D12_STENCIL_OP_ZERO;
        case StencilOp::Replace:  return D3D12_STENCIL_OP_REPLACE;
        case StencilOp::Incr:     return D3D12_STENCIL_OP_INCR_SAT;
        case StencilOp::Decr:     return D3D12_STENCIL_OP_DECR_SAT;
        case StencilOp::IncrWrap: return D3D12_STENCIL_OP_INCR;
        case StencilOp::DecrWrap: return D3D12_STENCIL_OP_DECR;
    }
    return D3D12_STENCIL_OP_KEEP;
}

D3D12_BLEND ToDxBlend(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero:             return D3D12_BLEND_ZERO;
        case BlendFactor::One:              return D3D12_BLEND_ONE;
        case BlendFactor::SrcAlpha:         return D3D12_BLEND_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
        case BlendFactor::SrcColor:         return D3D12_BLEND_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return D3D12_BLEND_INV_SRC_COLOR;
    }
    return D3D12_BLEND_ONE;
}

// list/strip 的区分留给 draw 时 IASetPrimitiveTopology（PSO 只约束拓扑类别）
D3D12_PRIMITIVE_TOPOLOGY_TYPE ToDxTopologyType(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::TriangleList:
        case PrimitiveType::TriangleStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        case PrimitiveType::Lines:         return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case PrimitiveType::Points:        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    }
    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
}

// D3D12 无 point 填充模式：以 Fill 承载并只警告一次
void WarnPointFillOnce() {
    static bool warned = false;
    if (!warned) {
        warned = true;
        LOGW("[DX12] PolygonMode::Point not supported by D3D12 fill modes; using Solid");
    }
}

// D3D12 无双面同时剔除（GL FrontAndBack=全剔除）：退化为仅剔背面并只警告一次
void WarnFrontAndBackOnce() {
    static bool warned = false;
    if (!warned) {
        warned = true;
        LOGW("[DX12] CullFace::FrontAndBack unsupported by D3D12; culling Back only");
    }
}

} // namespace

// filter/wrap → D3D12 描述（rhi 作用域定义，与 DXPipeline.hpp 声明配对；
// DXBackend 动态采样器堆与本文件静态表共用）
D3D12_FILTER DxFilterOf(TextureFilter filter) {
    switch (filter) {
        case TextureFilter::Linear:          return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        case TextureFilter::Nearest:         return D3D12_FILTER_MIN_MAG_MIP_POINT;
        case TextureFilter::LinearMipLinear: return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    }
    return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
}

D3D12_TEXTURE_ADDRESS_MODE DxAddressOf(TextureWrap wrap) {
    switch (wrap) {
        case TextureWrap::Repeat:         return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case TextureWrap::ClampToEdge:    return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case TextureWrap::ClampToBorder:  return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    }
    return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}

bool PSOKey::operator==(const PSOKey& o) const {
    if (stateHash != o.stateHash || colorCount != o.colorCount ||
        depth != o.depth || samples != o.samples)
        return false;
    for (uint32_t i = 0; i < colorCount && i < 8; ++i)
        if (color[i] != o.color[i]) return false;
    return true;
}

size_t PSOKeyHash::operator()(const PSOKey& k) const {
    uint32_t h = 2166136261u;
    auto mix = [&h](uint32_t v) { h ^= v; h *= 16777619u; };
    mix(k.stateHash);
    mix(k.colorCount);
    // 仅哈希前 colorCount 个槽位，与 operator== 语义一致（尾部槽位允许未初始化）
    for (uint32_t i = 0; i < k.colorCount && i < 8; ++i)
        mix(static_cast<uint32_t>(k.color[i]));
    mix(static_cast<uint32_t>(k.depth));
    mix(k.samples);
    return h;
}

int SamplerSlot(TextureFilter filter, TextureWrap wrap) {
    int fi = 0;
    switch (filter) {
        case TextureFilter::Linear:          fi = 0; break;
        case TextureFilter::Nearest:         fi = 1; break;
        case TextureFilter::LinearMipLinear: fi = 2; break;
    }
    int wi = 0;
    switch (wrap) {
        case TextureWrap::Repeat:        wi = 0; break;
        case TextureWrap::ClampToEdge:   wi = 1; break;
        case TextureWrap::ClampToBorder: wi = 2; break;
    }
    return fi * 3 + wi;
}

const D3D12_STATIC_SAMPLER_DESC* StaticSamplers(size_t& count) {
    // 槽位布局 f*3+w（寄存器编号与 _samplers.hlsli 别名一致）：
    //   0..2 Linear(Repeat/Clamp/Border)、3..5 Nearest、6..8 LinearMipLinear；
    // 槽 6 即默认 minFilter(LinearMipLinear)+Repeat。
    // s9：shadow map 硬件比较采样器（Task 10b Shadow 组），贴 GL Nearest 行为取
    //   MIN_MAG_MIP_POINT + LESS_EQUAL；wrap=ClampToBorder+OPAQUE_WHITE（越界=最远深度
    //   =受光，与 GL/VK borderColor 1.0 语义一致）。仅 Shadow 组的 shadowMap 使用。
    // s10：cubemap LOD 对齐采样器（SkyBox 组）：GL/VK 参考实现（NVIDIA）对立方体
    //   纹理的隐式 LOD 约定比 D3D12 高约 +0.28（实测参考输出恒为纯 mip1，DX12 为
    //   0.72 混合），此偏差属跨 API cube-LOD 公式差异而非着色器语义差。镜像树以
    //   带 MipLODBias 的专用别名吸收，避免全局改 s6 波及其余样例。
    // w=ClampToBorder 的三个槽位（2/5/8）不进静态表：静态采样器边框色只有黑/白，
    // 任意 borderColor 由 DXRenderer 动态 SAMPLER 堆在 bind 路径按槽位写入
    // （未覆盖时堆内预填 OPAQUE_WHITE，行为与旧静态表一致）。非 border 组合继续走根签名静态表。
    // 表按有效条目紧凑填充（寄存器编号仍为 f*3+w 与 9/10）：数组容量必须 ≥ 条目数，
    // 寄存器编号来自 MakeSampler 参数与数组下标无关
    static const std::array<D3D12_STATIC_SAMPLER_DESC, 9> table = [] {
        constexpr TextureFilter filters[3] = {TextureFilter::Linear, TextureFilter::Nearest,
                                              TextureFilter::LinearMipLinear};
        constexpr TextureWrap wraps[2] = {TextureWrap::Repeat, TextureWrap::ClampToEdge};
        std::array<D3D12_STATIC_SAMPLER_DESC, 9> t{};
        size_t n = 0;
        for (int f = 0; f < 3; ++f)
            for (int w = 0; w < 2; ++w)
                t[n++] =
                    MakeSampler(DxFilterOf(filters[f]), DxAddressOf(wraps[w]),
                                static_cast<UINT>(f * 3 + w));
        t[n++] = MakeSampler(D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT,
                             DxAddressOf(TextureWrap::ClampToBorder), 9,
                             D3D12_COMPARISON_FUNC_LESS_EQUAL);
        t[n] = MakeSampler(DxFilterOf(TextureFilter::LinearMipLinear),
                           DxAddressOf(TextureWrap::Repeat), 10);
        t[n].MipLODBias = 0.28f;
        ++n;
        t[n] = MakeSampler(DxFilterOf(TextureFilter::LinearMipLinear),
                           DxAddressOf(TextureWrap::Repeat), 11);
        t[n].MipLODBias = 0.45f;
        ++n;
        return t;
    }();
    count = table.size();
    return table.data();
}

bool CreateSharedRootSignature(ID3D12Device* device, ComPtr<ID3D12RootSignature>& out) {
    size_t samplerCount = 0;
    const D3D12_STATIC_SAMPLER_DESC* samplers = StaticSamplers(samplerCount);

    // param0：根 CBV(b0)，Task 6 用 SetGraphicsRootConstantBufferView 直挂 UBO ring 槽地址
    D3D12_ROOT_PARAMETER params[2]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;   // b0，与 _uniform_block.hlsli 对应
    params[0].Descriptor.RegisterSpace = 0;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // param1：描述符表 SRV t0..t127，片元采样纹理（本仓库全部纹理采样都在 FS）
    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 128;
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges = &srvRange;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 2;
    desc.pParameters = params;
    desc.NumStaticSamplers = static_cast<UINT>(samplerCount);
    desc.pStaticSamplers = samplers;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &errBlob);
    if (FAILED(hr)) {
        const char* msg = errBlob.Get() ? static_cast<const char*>(errBlob->GetBufferPointer()) : "";
        LOGE("[DX12] serialize root signature failed hr=0x{:08X} {}", static_cast<uint32_t>(hr), msg);
        return false;
    }
    DX_CHECK(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                         IID_PPV_ARGS(&out)),
             "create root signature");
    return out.Get() != nullptr;
}

DXPipeline::DXPipeline(ID3D12Device* device, ID3D12RootSignature* rootSig,
                       const VertexLayout& layoutIn, std::shared_ptr<IShader> shader)
    : _device(device), _rootSignature(rootSig), _layout(layoutIn),
      _shader(std::dynamic_pointer_cast<DXShader>(std::move(shader))) {}

void* DXPipeline::handle() { return _last; }

uint32_t DXPipeline::stateHash() const {
    uint32_t h = 2166136261u;
    auto mix = [&h](uint32_t v) { h ^= v; h *= 16777619u; };
    mix(_blend ? 1u : 0u);
    mix(static_cast<uint32_t>(_blendSrc));
    mix(static_cast<uint32_t>(_blendDst));
    mix(_depthTest ? 1u : 0u);
    mix(_depthMask ? 1u : 0u);
    mix(static_cast<uint32_t>(_depthFunc));
    mix(_stencilTest ? 1u : 0u);
    mix(static_cast<uint32_t>(_stencilFunc));
    mix(static_cast<uint32_t>(_stencilRef));
    mix(_stencilCompareMask);
    mix(_stencilWriteMask);
    mix(static_cast<uint32_t>(_stencilFail));
    mix(static_cast<uint32_t>(_stencilDepthFail));
    mix(static_cast<uint32_t>(_stencilPass));
    mix(_cullEnable ? 1u : 0u);
    mix(static_cast<uint32_t>(_cullFace));
    mix(_frontFaceCCW ? 1u : 0u);
    mix(static_cast<uint32_t>(_polygonMode));
    mix(_multisample ? 1u : 0u);
    mix(static_cast<uint32_t>(_primitive));
    for (const auto& e : _layout.elements) {
        mix(static_cast<uint32_t>(e.format));
        mix(static_cast<uint32_t>(e.semantic));
        mix(static_cast<uint32_t>(e.binding));
        mix(static_cast<uint32_t>(e.inputRate));
        mix(static_cast<uint32_t>(e.offset));
        mix(static_cast<uint32_t>(e.stride));
    }
    return h;
}

ID3D12PipelineState* DXPipeline::pipelineFor(const PSOKey& key) {
    auto it = _cache.find(key);
    if (it != _cache.end()) {
        _last = it->second.Get();
        return _last;
    }
    ComPtr<ID3D12PipelineState> pso;
    if (!createGraphicsPipeline(key, pso)) return nullptr;
    auto inserted = _cache.emplace(key, std::move(pso));
    _last = inserted.first->second.Get();
    return _last;
}

bool DXPipeline::createGraphicsPipeline(const PSOKey& key, ComPtr<ID3D12PipelineState>& out) {
    if (!_shader || !_shader->valid()) {
        LOGE("[DX12] create PSO: shader not compiled");
        return false;
    }
    // VS/PS 必需；GS 有则三阶段
    ComPtr<ID3DBlob> vs = _shader->moduleFor(ShaderStage::Vertex);
    ComPtr<ID3DBlob> ps = _shader->moduleFor(ShaderStage::Fragment);
    ComPtr<ID3DBlob> gs = _shader->moduleFor(ShaderStage::Geometry);
    if (!vs.Get() || !ps.Get()) {
        LOGE("[DX12] create PSO: missing VS/PS bytecode");
        return false;
    }

    // 顶点装配：HLSL 输入语义统一 TEXCOORD<i>（i=element.semantic），
    // InputSlot=element.binding（GL 分离 VBO → 多槽），实例数据 PerInstance 步进 1
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputs;
    inputs.reserve(_layout.elements.size());
    for (const auto& e : _layout.elements) {
        D3D12_INPUT_ELEMENT_DESC d{};
        d.SemanticName = "TEXCOORD";
        d.SemanticIndex = static_cast<UINT>(e.semantic);
        d.Format = ToDxVertexFormat(e.format);
        d.InputSlot = e.binding;
        d.AlignedByteOffset = e.offset >= 0 ? static_cast<UINT>(e.offset) : 0;
        const bool perInstance = e.inputRate == VertexInputRate::PerInstance;
        d.InputSlotClass = perInstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                                       : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        d.InstanceDataStepRate = perInstance ? 1u : 0u;
        inputs.push_back(d);
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = _rootSignature;
    psoDesc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    psoDesc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    if (gs.Get())
        psoDesc.GS = {gs->GetBufferPointer(), gs->GetBufferSize()};
    psoDesc.InputLayout = {inputs.data(), static_cast<UINT>(inputs.size())};

    D3D12_RASTERIZER_DESC& rs = psoDesc.RasterizerState;
    switch (_polygonMode) {
        case PolygonMode::Line:  rs.FillMode = D3D12_FILL_MODE_WIREFRAME; break;
        case PolygonMode::Point:
            WarnPointFillOnce();
            [[fallthrough]];
        default:                 rs.FillMode = D3D12_FILL_MODE_SOLID; break;
    }
    if (!_cullEnable) rs.CullMode = D3D12_CULL_MODE_NONE;
    else if (_cullFace == CullFace::Front) rs.CullMode = D3D12_CULL_MODE_FRONT;
    else {
        if (_cullFace == CullFace::FrontAndBack)
            WarnFrontAndBackOnce();
        rs.CullMode = D3D12_CULL_MODE_BACK;
    }
    rs.FrontCounterClockwise = _frontFaceCCW ? TRUE : FALSE;  // GL 惯例 CCW 正面
    rs.DepthClipEnable = TRUE;

    // GL 语义对齐（同 VKPipeline）：测试关、写开时仍需写深度 → 强制开测试 + ALWAYS
    D3D12_DEPTH_STENCIL_DESC& ds = psoDesc.DepthStencilState;
    ds.DepthEnable = (_depthTest || _depthMask) ? TRUE : FALSE;
    ds.DepthWriteMask = _depthMask ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    ds.DepthFunc = _depthTest ? ToDxCompare(_depthFunc) : D3D12_COMPARISON_FUNC_ALWAYS;
    ds.StencilEnable = _stencilTest ? TRUE : FALSE;
    ds.StencilReadMask = static_cast<UINT8>(_stencilCompareMask);
    ds.StencilWriteMask = static_cast<UINT8>(_stencilWriteMask);
    const D3D12_DEPTH_STENCILOP_DESC sop{
        ToDxStencilOp(_stencilFail), ToDxStencilOp(_stencilDepthFail),
        ToDxStencilOp(_stencilPass), ToDxCompare(_stencilFunc)};
    ds.FrontFace = sop;
    ds.BackFace = sop;   // 接口为单面模板状态，前后同配（同 VK）

    D3D12_BLEND_DESC& bd = psoDesc.BlendState;
    bd.AlphaToCoverageEnable = FALSE;
    bd.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC rtbd{
        _blend ? TRUE : FALSE,
        FALSE,
        ToDxBlend(_blendSrc),
        ToDxBlend(_blendDst),
        D3D12_BLEND_OP_ADD,
        ToDxBlend(_blendSrc),
        ToDxBlend(_blendDst),
        D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL};
    const UINT blendTargets = key.colorCount > 0 ? key.colorCount : 1;
    for (UINT i = 0; i < blendTargets && i < 8; ++i) bd.RenderTarget[i] = rtbd;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = ToDxTopologyType(_primitive);
    psoDesc.NumRenderTargets = key.colorCount;   // 0 = 纯深度 pass 合法
    for (UINT i = 0; i < key.colorCount && i < 8; ++i)
        psoDesc.RTVFormats[i] = key.color[i];
    psoDesc.DSVFormat = key.depth;
    psoDesc.SampleDesc.Count = key.samples > 0 ? key.samples : 1;
    psoDesc.SampleDesc.Quality = 0;

    DX_CHECK(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&out)),
             "create graphics pipeline state");
    return out.Get() != nullptr;
}

} // namespace rhi
