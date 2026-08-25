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

} // namespace rhi
