#include "rhi/dx12/DXRenderTarget.hpp"
#include "rhi/dx12/DXTexture3D.hpp"
#include "rhi/dx12/DXFormat.hpp"
#include "base/Log.hpp"
#include <algorithm>

namespace rhi {

namespace {

// PSO DSVFormat / ClearDepthStencilView 用 typed 深度格式（资源侧 TYPELESS）
DXGI_FORMAT DsvFormatOf(TextureFormat f) {
    switch (f) {
        case TextureFormat::Depth32F:        return DXGI_FORMAT_D32_FLOAT;
        case TextureFormat::Depth24Stencil8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default:                             return DXGI_FORMAT_D24_UNORM_S8_UINT;
    }
}

D3D12_RESOURCE_BARRIER MakeTransition(ID3D12Resource* res,
                                      D3D12_RESOURCE_STATES before,
                                      D3D12_RESOURCE_STATES after,
                                      UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = res;
    b.Transition.StateBefore = before;
    b.Transition.StateAfter = after;
    b.Transition.Subresource = subresource;
    return b;
}

} // namespace

DXRenderTarget::DXRenderTarget(ID3D12Device* device, ID3D12CommandQueue* queue,
                               ID3D12CommandAllocator* uploadAlloc, ID3D12Fence* uploadFence,
                               HANDLE fenceEvent)
    : _device(device)
    , _queue(queue)
    , _uploadAlloc(uploadAlloc)
    , _uploadFence(uploadFence)
    , _fenceEvent(fenceEvent) {}

DXRenderTarget::~DXRenderTarget() { release(); }

bool DXRenderTarget::create(int width, int height) {
    FramebufferDesc desc;
    desc.width = width;
    desc.height = height;
    FramebufferAttachment color;
    color.type = AttachmentType::Color;
    color.format = TextureFormat::RGBA8;
    desc.attachments.push_back(color);
    FramebufferAttachment depth;
    depth.type = AttachmentType::DepthStencil;
    depth.format = TextureFormat::Depth24Stencil8;
    desc.attachments.push_back(depth);
    return create(desc);
}

bool DXRenderTarget::create(const FramebufferDesc& desc) {
    release();
    if (!_device || desc.width <= 0 || desc.height <= 0) return false;
    _width = desc.width;
    _height = desc.height;
    _samples = desc.samples > 0 ? static_cast<UINT>(desc.samples) : 1;

    // 附件纹理经 DXTexture2D::createEmpty 落到常驻态：非 MSAA 颜色=PSHR、
    // MSAA 颜色=RENDER_TARGET（DENY_SHADER_RESOURCE）、深度=DEPTH_WRITE，
    // 与 BeginPass/EndPass 的状态机约定一致
    bool depthTaken = false;   // 多余深度附件忽略（对齐 VK if (_depthAttachment) continue）
    for (const auto& att : desc.attachments) {
        if (att.type == AttachmentType::Color) {
            auto tex = std::make_shared<DXTexture2D>(_device, _queue, _uploadAlloc,
                                                     _uploadFence, _fenceEvent);
            TextureDesc td;
            td.format = att.format;
            td.minFilter = att.minFilter;
            td.magFilter = att.magFilter;
            td.wrapS = att.wrapS;
            td.wrapT = att.wrapT;
            if (_samples > 1) {
                td.multisample = true;
                td.samples = static_cast<int>(_samples);
            }
            if (!tex->createEmpty(td, _width, _height)) {
                LOGE("[DX12] RT color attachment create failed");
                return false;
            }
            tex->setBorderColor(att.borderColor);
            if (tex->isMsaa()) _msaaColors.push_back(tex);
            else _colors.push_back(tex);
        } else if (!depthTaken) {
            auto tex = std::make_shared<DXTexture2D>(_device, _queue, _uploadAlloc,
                                                     _uploadFence, _fenceEvent);
            TextureDesc td;
            td.format = att.format;
            td.minFilter = att.minFilter;
            td.magFilter = att.magFilter;
            td.wrapS = att.wrapS;
            td.wrapT = att.wrapT;
            if (_samples > 1) {
                td.multisample = true;
                td.samples = static_cast<int>(_samples);
            }
            if (!tex->createEmpty(td, _width, _height)) {
                LOGE("[DX12] RT depth attachment create failed");
                return false;
            }
            tex->setBorderColor(att.borderColor);
            _depth = tex;
            _depthStencilFormat = DsvFormatOf(att.format);
            depthTaken = true;
        }
    }

    // 允许零附件基座 RT（PointLightShadow 先建空基座再 attachDepthCube 逐面挂接）；
    // RTV/DSV 堆按需创建，OM 目标完全由挂接状态决定

    const UINT rtvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_DESCRIPTOR_HEAP_DESC rhd{};
    rhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rhd.NumDescriptors = std::max<UINT>(1, static_cast<UINT>(_colors.size() + _msaaColors.size()));
    DX_CHECK(_device->CreateDescriptorHeap(&rhd, IID_PPV_ARGS(&_rtvHeap)), "create rt rtv heap");
    if (!_rtvHeap.Get()) return false;
    auto base = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
    UINT slot = 0;
    for (auto& t : _colors) {
        D3D12_CPU_DESCRIPTOR_HANDLE h = base;
        h.ptr += static_cast<SIZE_T>(slot++) * rtvInc;
        _device->CreateRenderTargetView(t->resource(), nullptr, h);   // 颜色资源已 typed，视图用存储格式
    }
    for (auto& t : _msaaColors) {
        D3D12_CPU_DESCRIPTOR_HANDLE h = base;
        h.ptr += static_cast<SIZE_T>(slot++) * rtvInc;
        _device->CreateRenderTargetView(t->resource(), nullptr, h);
    }
    if (_depth.get()) {
        D3D12_DESCRIPTOR_HEAP_DESC dhd{};
        dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dhd.NumDescriptors = 1;
        DX_CHECK(_device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&_dsvHeap)), "create rt dsv heap");
        if (!_dsvHeap.Get()) return false;
        D3D12_DEPTH_STENCIL_VIEW_DESC dd{};
        dd.Format = _depthStencilFormat;
        dd.ViewDimension = _depth->isMsaa() ? D3D12_DSV_DIMENSION_TEXTURE2DMS
                                            : D3D12_DSV_DIMENSION_TEXTURE2D;
        _device->CreateDepthStencilView(_depth->resource(), &dd,
                                        _dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    _valid = true;
    return true;
}

bool DXRenderTarget::attachCubeFace(ITexture3D* cube, int face, int mip) {
    auto* c3d = dynamic_cast<DXTexture3D*>(cube);
    if (!c3d || !c3d->valid() || !c3d->isCube() || face < 0 || face > 5 ||
        mip < 0 || static_cast<UINT>(mip) >= c3d->mipLevels()) {
        LOGW("[DX12] attachCubeFace: invalid cube/face/mip");
        return false;
    }
    // 延迟生效：仅记录挂接状态，OM 目标在 Renderer 下次 flushOmTargets 时装配
    // （App 模式=setRenderTarget→attachCubeFace→clearColor→draw，时序天然成立）
    _cube = c3d;
    _face = face;
    _mip = mip;
    _cubeIsDepth = c3d->isDepth();
    return true;
}

bool DXRenderTarget::attachDepthCube(ITexture3D* cube, int mip) {
    (void)mip;   // 分层写入无 GS 支撑；样例随后逐面 attachCubeFace 生效
    auto* c3d = dynamic_cast<DXTexture3D*>(cube);
    if (!c3d || !c3d->valid() || !c3d->isDepth() || !c3d->isCube()) {
        LOGW("[DX12] attachDepthCube: not a depth cube");
        return false;
    }
    _cube = c3d;
    _face = -1;   // 待首个 attachCubeFace 选面
    _mip = 0;
    _cubeIsDepth = true;
    return true;
}

void* DXRenderTarget::colorTexture() {
    if (!_colors.empty()) return _colors[0]->resource();
    if (!_msaaColors.empty()) return _msaaColors[0]->resource();
    return nullptr;
}

ITexture2D* DXRenderTarget::colorTexture2D(int attachment) {
    if (attachment < 0) return nullptr;
    const size_t idx = static_cast<size_t>(attachment);
    if (idx < _colors.size()) return _colors[idx].get();
    const size_t msaaIdx = idx - _colors.size();
    if (msaaIdx < _msaaColors.size()) return _msaaColors[msaaIdx].get();
    return nullptr;
}

ITexture2D* DXRenderTarget::depthTexture2D() { return _depth.get(); }

bool DXRenderTarget::resolveTo(IRenderTarget&) {
    return true;   // MSAA resolve 由 blitFramebuffer 的 ResolveSubresource 承担（同 VK no-op）
}

void* DXRenderTarget::handle() { return colorTexture(); }

void DXRenderTarget::release() {
    _depth.reset();
    _msaaColors.clear();
    _colors.clear();
    _rtvHeap = {};
    _dsvHeap = {};
    _cube = nullptr;
    _face = -1;
    _mip = 0;
    _cubeIsDepth = false;
    _colorsInRT = false;
    _depthInWrite = true;
    _cubeFaceInRT = false;
    _depthStencilFormat = DXGI_FORMAT_UNKNOWN;
    _width = 0;
    _height = 0;
    _samples = 1;
    _valid = false;
}

uint32_t DXRenderTarget::colorCount() const {
    if (!_valid) return 0;
    if (_cube && _cubeIsDepth && _face >= 0) return 0;   // 纯深度 pass 合法（NumRenderTargets=0）
    if (_cube && !_cubeIsDepth && _face >= 0) return 1;  // cube 面即唯一颜色目标（PBR IBL capture）
    return static_cast<uint32_t>(_colors.size() + _msaaColors.size());
}

DXGI_FORMAT DXRenderTarget::colorFormat(uint32_t i) const {
    if (!_valid) return DXGI_FORMAT_R8G8B8A8_UNORM;
    if (_cube && !_cubeIsDepth && _face >= 0) {
        return i == 0 ? _cube->rtvFormat() : DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    const size_t total = _colors.size() + _msaaColors.size();
    if (i >= total) return DXGI_FORMAT_R8G8B8A8_UNORM;
    const size_t nonMsaa = _colors.size();
    return i < nonMsaa ? _colors[i]->storageFormat()
                       : _msaaColors[i - nonMsaa]->storageFormat();
}

DXGI_FORMAT DXRenderTarget::depthFormat() const {
    if (!_valid) return DXGI_FORMAT_UNKNOWN;
    if (_cube && _cubeIsDepth && _face >= 0) return _cube->dsvFormat();
    if (_depth.get()) return _depthStencilFormat;
    return DXGI_FORMAT_UNKNOWN;
}

bool DXRenderTarget::hasDepthAttachment() const {
    return depthFormat() != DXGI_FORMAT_UNKNOWN;
}

void DXRenderTarget::renderDims(int& w, int& h) const {
    w = _width;
    h = _height;
    if (_cube && _face >= 0 && _mip > 0) {
        w = std::max(1, _width >> _mip);
        h = std::max(1, _height >> _mip);
    }
}

void DXRenderTarget::BeginPass(ID3D12GraphicsCommandList* cmd) {
    if (!_valid || !cmd) return;
    std::vector<D3D12_RESOURCE_BARRIER> bars;
    if (!_colorsInRT) {
        for (auto& t : _colors) {
            if (t->resource()) bars.push_back(MakeTransition(
                t->resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET));
        }
        _colorsInRT = true;
    }
    if (_cube && !_cubeIsDepth && _face >= 0 && !_cubeFaceInRT) {
        bars.push_back(MakeTransition(
            _cube->resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            _cube->subresource(_face, _mip)));   // 仅当前面子资源参与转移（其余面保持可采样）
        _cubeFaceInRT = true;
    }
    ID3D12Resource* depthRes = nullptr;
    if (_cube && _cubeIsDepth && _face >= 0) depthRes = _cube->resource();
    else if (_depth.get()) depthRes = _depth->resource();
    if (depthRes && !_depthInWrite) {
        bars.push_back(MakeTransition(depthRes,
                                      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                      D3D12_RESOURCE_STATE_DEPTH_WRITE));
        _depthInWrite = true;
    }
    if (!bars.empty()) cmd->ResourceBarrier(static_cast<UINT>(bars.size()), bars.data());
}

void DXRenderTarget::EndPass(ID3D12GraphicsCommandList* cmd) {
    if (!_valid || !cmd) return;
    std::vector<D3D12_RESOURCE_BARRIER> bars;
    if (_colorsInRT) {
        for (auto& t : _colors) {
            if (t->resource()) bars.push_back(MakeTransition(
                t->resource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
        }
        _colorsInRT = false;
    }
    if (_cube && !_cubeIsDepth && _face >= 0 && _cubeFaceInRT) {
        bars.push_back(MakeTransition(
            _cube->resource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            _cube->subresource(_face, _mip)));
        _cubeFaceInRT = false;
    }
    ID3D12Resource* depthRes = nullptr;
    if (_cube && _cubeIsDepth && _face >= 0) depthRes = _cube->resource();
    else if (_depth.get()) depthRes = _depth->resource();
    if (depthRes && _depthInWrite) {
        // 回 PSHR：pass 间深度调试 quad 采样、阴影图采样均依赖常驻可读态
        bars.push_back(MakeTransition(depthRes,
                                      D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
        _depthInWrite = false;
    }
    if (!bars.empty()) cmd->ResourceBarrier(static_cast<UINT>(bars.size()), bars.data());
}

void DXRenderTarget::GetOmTargets(std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& rtvs,
                                  D3D12_CPU_DESCRIPTOR_HANDLE& dsv, bool& hasDsv) const {
    rtvs.clear();
    hasDsv = false;
    if (!_valid) return;
    const bool cubeDepthFace = _cube && _cubeIsDepth && _face >= 0;
    if (cubeDepthFace) {
        auto h = _cube->dsvFace(_face);
        if (h.ptr) {
            dsv = h;
            hasDsv = true;
        }
        return;
    }
    const UINT rtvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    if (_cube && !_cubeIsDepth && _face >= 0) {
        auto h = _cube->rtvFace(_face, _mip);
        if (h.ptr) rtvs.push_back(h);
    } else {
        auto base = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
        UINT n = 0;
        for (size_t i = 0; i < _colors.size() + _msaaColors.size(); ++i) {
            D3D12_CPU_DESCRIPTOR_HANDLE h = base;
            h.ptr += static_cast<SIZE_T>(n++) * rtvInc;
            rtvs.push_back(h);
        }
    }
    if (_depth.get() && _dsvHeap.Get()) {
        dsv = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
        hasDsv = true;
    }
}

void DXRenderTarget::ClearAll(ID3D12GraphicsCommandList* cmd, const std::array<float, 4>& cc) {
    if (!_valid || !cmd) return;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
    bool hasDsv = false;
    GetOmTargets(rtvs, dsv, hasDsv);
    for (const auto& h : rtvs) {
        cmd->ClearRenderTargetView(h, cc.data(), 0, nullptr);
    }
    if (hasDsv) {
        D3D12_CLEAR_FLAGS flags = D3D12_CLEAR_FLAG_DEPTH;
        const DXGI_FORMAT fmt = depthFormat();
        if (fmt == DXGI_FORMAT_D24_UNORM_S8_UINT) flags |= D3D12_CLEAR_FLAG_STENCIL;
        cmd->ClearDepthStencilView(dsv, flags, 1.0f, 0, 0, nullptr);
    }
}

} // namespace rhi
