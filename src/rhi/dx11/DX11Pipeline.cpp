#include "rhi/dx11/DX11Backend.hpp"
#include "base/Log.hpp"
#include <vector>

namespace rhi {

namespace {

DXGI_FORMAT ToDx11VertexFormat(VertexElement::Format format) {
    switch (format) {
        case VertexElement::Float2: return DXGI_FORMAT_R32G32_FLOAT;
        case VertexElement::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
        case VertexElement::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case VertexElement::Int4:   return DXGI_FORMAT_R32G32B32A32_SINT;
    }
    return DXGI_FORMAT_R32G32B32_FLOAT;
}

// D3D11 无 point 填充模式：以 Fill 承载并只警告一次（对照 DXPipeline::WarnPointFillOnce）
void WarnPointFillOnce() {
    static bool warned = false;
    if (!warned) {
        warned = true;
        LOGW("[DX11] PolygonMode::Point not supported by D3D11 fill modes; using Solid");
    }
}

// D3D11 无双面同时剔除（GL FrontAndBack=全剔除）：退化为仅剔背面并只警告一次
void WarnFrontAndBackOnce() {
    static bool warned = false;
    if (!warned) {
        warned = true;
        LOGW("[DX11] CullFace::FrontAndBack unsupported by D3D11; culling Back only");
    }
}

} // namespace

DX11Pipeline::DX11Pipeline(ID3D11Device* device, const VertexLayout& layoutIn,
                           std::shared_ptr<IShader> shader)
    : _device(device), _layout(layoutIn),
      _shader(std::dynamic_pointer_cast<DX11Shader>(std::move(shader))) {
    if (!_device || !_shader || !_shader->valid()) return;
    // InputLayout 由 VS 字节码签名校验创建：HLSL 输入语义统一 TEXCOORD<i>
    // （i=element.semantic），InputSlot=element.binding（GL 分离 VBO → 多槽，
    // 同 DX12 的 InputSlotClass 装配约定），实例数据 PerInstance 步进 1
    auto vs = _shader->moduleFor(ShaderStage::Vertex);
    if (!vs.Get()) {
        LOGE("[DX11] create input layout: missing VS bytecode");
        return;
    }
    std::vector<D3D11_INPUT_ELEMENT_DESC> inputs;
    inputs.reserve(_layout.elements.size());
    for (const auto& e : _layout.elements) {
        D3D11_INPUT_ELEMENT_DESC d{};
        d.SemanticName = "TEXCOORD";
        d.SemanticIndex = static_cast<UINT>(e.semantic);
        d.Format = ToDx11VertexFormat(e.format);
        d.InputSlot = e.binding;
        d.AlignedByteOffset = e.offset >= 0 ? static_cast<UINT>(e.offset) : 0;
        const bool perInstance = e.inputRate == VertexInputRate::PerInstance;
        d.InputSlotClass = perInstance ? D3D11_INPUT_PER_INSTANCE_DATA
                                       : D3D11_INPUT_PER_VERTEX_DATA;
        d.InstanceDataStepRate = perInstance ? 1u : 0u;
        inputs.push_back(d);
    }
    DX11_CHECK(_device->CreateInputLayout(inputs.data(), static_cast<UINT>(inputs.size()),
                                          vs->GetBufferPointer(), vs->GetBufferSize(),
                                          &_inputLayout),
               "create input layout");
    if (!_inputLayout.Get()) {
        LOGE("[DX11] input layout creation failed (semantic/layout mismatch?)");
    }
}

// 着色器对象按字节码懒创建一次（D3D11 与 D3D12 不同：字节码须经 Create*Shader
// 物化为接口对象才能绑定到管线阶段）
void DX11Pipeline::ensureShaderObjects() {
    if (_shadersReady || !_device || !_shader || !_shader->valid()) return;
    auto vs = _shader->moduleFor(ShaderStage::Vertex);
    auto ps = _shader->moduleFor(ShaderStage::Fragment);
    auto gs = _shader->moduleFor(ShaderStage::Geometry);
    if (vs.Get()) {
        DX11_CHECK(_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(),
                                               nullptr, &_vs),
                   "create vertex shader");
    }
    if (ps.Get()) {
        DX11_CHECK(_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(),
                                              nullptr, &_ps),
                   "create pixel shader");
    }
    if (gs.Get()) {   // GS 可选（Explode/NormalLine 等几何着色器样例）
        DX11_CHECK(_device->CreateGeometryShader(gs->GetBufferPointer(), gs->GetBufferSize(),
                                                 nullptr, &_gs),
                   "create geometry shader");
    }
    _shadersReady = true;
}

void DX11Pipeline::bindShaders(ID3D11DeviceContext* ctx) {
    ensureShaderObjects();
    ctx->IASetInputLayout(_inputLayout.Get());
    ctx->VSSetShader(_vs.Get(), nullptr, 0);
    ctx->PSSetShader(_ps.Get(), nullptr, 0);
    // 无 GS 时传 nullptr 合法（显式解绑该阶段）
    ctx->GSSetShader(_gs.Get(), nullptr, 0);
}

// 状态指纹：与 DX12 DXPipeline::stateHash 同字段口径（FNV-1a 逐项混入）。
// 差异点：_stencilRef 参与键——DX11 无 DX12 OMSetStencilRef 式独立 ref 下发，
// ref 变化必须落到不同的状态对象（OMSetDepthStencilState 第二参数随对象一起换）。
uint32_t DX11Pipeline::stateHash() const {
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
    // _multisample 不混入：D3D11 的 MSAA 是 RT 资源属性（SampleDesc）而非状态对象
    // 输入，入键只产生永不消费的冗余变体（评审 Minor-2；成员保留供未来 RT 协商）
    mix(static_cast<uint32_t>(_primitive));
    return h;
}

// 未命中按当前成员构建 RS/Blend/DS 三元组。字段翻译口径对照
// src/rhi/dx12/DXPipeline.cpp createGraphicsPipeline（RS/BlendState/DepthStencilState）
// 逐字段一致——两代 API 拆成独立状态对象但语义相同。
// 任一创建失败返回 nullptr 且不入缓存（评审 Minor-1：null 对象不得被永久复用；
// 对照 DXPipeline::pipelineFor 未命中且创建失败时同样不产生缓存项）。
DX11Pipeline::StateObjects* DX11Pipeline::statesFor(uint32_t hash) {
    auto it = _stateCache.find(hash);
    if (it != _stateCache.end()) return &it->second;

    StateObjects st{};

    // ---- 光栅化（对照 psoDesc.RasterizerState）----
    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = D3D11_FILL_SOLID;
    switch (_polygonMode) {
        case PolygonMode::Line:  rs.FillMode = D3D11_FILL_WIREFRAME; break;
        case PolygonMode::Point:
            WarnPointFillOnce();
            [[fallthrough]];
        default:                 rs.FillMode = D3D11_FILL_SOLID; break;
    }
    if (!_cullEnable) rs.CullMode = D3D11_CULL_NONE;
    else if (_cullFace == CullFace::Front) rs.CullMode = D3D11_CULL_FRONT;
    else {
        if (_cullFace == CullFace::FrontAndBack)
            WarnFrontAndBackOnce();
        rs.CullMode = D3D11_CULL_BACK;
    }
    rs.FrontCounterClockwise = _frontFaceCCW ? TRUE : FALSE;   // GL 惯例 CCW 正面
    rs.DepthBias = 0;
    rs.DepthBiasClamp = 0.0f;
    rs.SlopeScaledDepthBias = 0.0f;
    rs.DepthClipEnable = TRUE;
    rs.MultisampleEnable = FALSE;
    rs.AntialiasedLineEnable = FALSE;
    DX11_CHECK(_device->CreateRasterizerState(&rs, &st.rs), "create rasterizer state");

    // ---- 深度/模板（对照 psoDesc.DepthStencilState）----
    // GL 语义对齐（同 VKPipeline）：测试关、写开时仍需写深度 → 强制开测试 + ALWAYS
    D3D11_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = (_depthTest || _depthMask) ? TRUE : FALSE;
    ds.DepthWriteMask = _depthMask ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    ds.DepthFunc = _depthTest ? Dx11Compare(_depthFunc) : D3D11_COMPARISON_ALWAYS;
    ds.StencilEnable = _stencilTest ? TRUE : FALSE;
    ds.StencilReadMask = static_cast<UINT8>(_stencilCompareMask);
    ds.StencilWriteMask = static_cast<UINT8>(_stencilWriteMask);
    const D3D11_DEPTH_STENCILOP_DESC sop{
        Dx11StencilOp(_stencilFail), Dx11StencilOp(_stencilDepthFail),
        Dx11StencilOp(_stencilPass), Dx11Compare(_stencilFunc)};
    ds.FrontFace = sop;
    ds.BackFace = sop;   // 接口为单面模板状态，前后同配（同 VK/DX12）
    DX11_CHECK(_device->CreateDepthStencilState(&ds, &st.ds), "create depth stencil state");

    // ---- 混合（对照 psoDesc.BlendState，GL SrcAlpha/OneMinusSrcAlpha 语义）----
    D3D11_BLEND_DESC bd{};
    bd.AlphaToCoverageEnable = FALSE;
    bd.IndependentBlendEnable = FALSE;
    bd.RenderTarget[0].BlendEnable = _blend ? TRUE : FALSE;
    bd.RenderTarget[0].SrcBlend = Dx11Blend(_blendSrc);
    bd.RenderTarget[0].DestBlend = Dx11Blend(_blendDst);
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = Dx11Blend(_blendSrc);
    bd.RenderTarget[0].DestBlendAlpha = Dx11Blend(_blendDst);
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    DX11_CHECK(_device->CreateBlendState(&bd, &st.blend), "create blend state");
    if (!st.rs.Get() || !st.ds.Get() || !st.blend.Get()) {
        LOGE("[DX11] state objects incomplete rs={} ds={} blend={} (not cached)",
             static_cast<void*>(st.rs.Get()), static_cast<void*>(st.ds.Get()),
             static_cast<void*>(st.blend.Get()));
        return nullptr;
    }

    return &_stateCache.emplace(hash, std::move(st)).first->second;
}

// 每 draw 全量下发（即时上下文无状态泄漏风险，对齐 DX12 "prepareDraw 自愈重设"）。
// 返回 false=状态对象缺失，调用方跳过本次 draw（同 DX12 PSO 为 null 的语义）
bool DX11Pipeline::bindStates(ID3D11DeviceContext* ctx) {
    const StateObjects* st = statesFor(stateHash());
    if (!st) return false;
    ctx->RSSetState(st->rs.Get());
    ctx->OMSetDepthStencilState(st->ds.Get(), static_cast<UINT>(_stencilRef));
    ctx->OMSetBlendState(st->blend.Get(), nullptr, 0xFFFFFFFFu);
    return true;
}

} // namespace rhi
