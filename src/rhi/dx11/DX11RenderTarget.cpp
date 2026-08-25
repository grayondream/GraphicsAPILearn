#include "rhi/dx11/DX11Backend.hpp"
#include "base/Log.hpp"
#include <algorithm>

namespace rhi {

namespace {

// DSV 视图 / ClearDepthStencilView 用 typed 深度格式（资源侧 TYPELESS）
DXGI_FORMAT DsvFormatOf(TextureFormat f) {
    switch (f) {
        case TextureFormat::Depth32F:        return DXGI_FORMAT_D32_FLOAT;
        case TextureFormat::Depth24Stencil8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default:                             return DXGI_FORMAT_D24_UNORM_S8_UINT;
    }
}

} // namespace

DX11RenderTarget::DX11RenderTarget(ID3D11Device* device, ID3D11DeviceContext* context)
    : _device(device), _context(context) {}

DX11RenderTarget::~DX11RenderTarget() { release(); }

bool DX11RenderTarget::create(int width, int height) {
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

bool DX11RenderTarget::create(const FramebufferDesc& desc) {
    release();
    if (!_device || desc.width <= 0 || desc.height <= 0) return false;
    _width = desc.width;
    _height = desc.height;
    _samples = desc.samples > 0 ? static_cast<UINT>(desc.samples) : 1;

    // 附件纹理经 DX11Texture2D::createEmpty 创建：非 MSAA 颜色 BIND_RT|SRV、
    // MSAA 颜色 RTV-only（不可采样，同 DX12 DENY_SHADER_RESOURCE 语义）、
    // 深度 TYPELESS 族 BIND_DSV|SRV——视图在此按用途补建
    bool depthTaken = false;   // 多余深度附件忽略（对齐 VK/DX12 if (_depthAttachment) continue）
    for (const auto& att : desc.attachments) {
        if (att.type == AttachmentType::Color) {
            auto tex = std::make_shared<DX11Texture2D>(_device, _context);
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
                LOGE("[DX11] RT color attachment create failed");
                return false;
            }
            tex->setBorderColor(att.borderColor);
            Dx11ComPtr<ID3D11RenderTargetView> rtv;
            D3D11_RENDER_TARGET_VIEW_DESC rd{};
            rd.Format = tex->storageFormat();
            rd.ViewDimension = tex->isMsaa() ? D3D11_RTV_DIMENSION_TEXTURE2DMS
                                             : D3D11_RTV_DIMENSION_TEXTURE2D;
            DX11_CHECK(_device->CreateRenderTargetView(
                           static_cast<ID3D11Texture2D*>(tex->handle()), &rd, &rtv),
                       "create rt color rtv");
            if (!rtv.Get()) return false;
            _rtvs.push_back(std::move(rtv));
            if (tex->isMsaa()) _msaaColors.push_back(std::move(tex));
            else _colors.push_back(std::move(tex));
        } else if (!depthTaken) {
            auto tex = std::make_shared<DX11Texture2D>(_device, _context);
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
                LOGE("[DX11] RT depth attachment create failed");
                return false;
            }
            tex->setBorderColor(att.borderColor);
            _dsvFormat = DsvFormatOf(att.format);
            D3D11_DEPTH_STENCIL_VIEW_DESC dd{};
            dd.Format = _dsvFormat;
            dd.ViewDimension = tex->isMsaa() ? D3D11_DSV_DIMENSION_TEXTURE2DMS
                                             : D3D11_DSV_DIMENSION_TEXTURE2D;
            DX11_CHECK(_device->CreateDepthStencilView(
                           static_cast<ID3D11Texture2D*>(tex->handle()), &dd, &_dsv),
                       "create rt dsv");
            if (!_dsv.Get()) return false;
            _depth = std::move(tex);
            depthTaken = true;
        }
    }

    // 允许零附件基座 RT（PointLightShadow 先建空基座再 attachDepthCube 逐面挂接）；
    // OM 目标完全由挂接状态决定
    _valid = true;
    return true;
}

bool DX11RenderTarget::attachCubeFace(ITexture3D* cube, int face, int mip) {
    auto* c3d = dynamic_cast<DX11Texture3D*>(cube);
    if (!c3d || !c3d->valid() || !c3d->isCube() || face < 0 || face > 5 ||
        mip < 0 || static_cast<UINT>(mip) >= c3d->mipLevels()) {
        LOGW("[DX11] attachCubeFace: invalid cube/face/mip");
        return false;
    }
    // 延迟生效：仅记录挂接状态，OM 目标在 Renderer 下次 flushOmTargets 时装配
    // （App 模式=setRenderTarget→attachCubeFace→clearColor→draw，时序天然成立；
    // 语义对齐 DX12 同名接口）
    _cube = c3d;
    _face = face;
    _mip = mip;
    _cubeIsDepth = c3d->isDepth();
    return true;
}

bool DX11RenderTarget::attachDepthCube(ITexture3D* cube, int mip) {
    (void)mip;   // 分层写入无 GS 支撑；样例随后逐面 attachCubeFace 生效（同 DX12 口径）
    auto* c3d = dynamic_cast<DX11Texture3D*>(cube);
    if (!c3d || !c3d->valid() || !c3d->isDepth() || !c3d->isCube()) {
        LOGW("[DX11] attachDepthCube: not a depth cube");
        return false;
    }
    _cube = c3d;
    _face = -1;   // 待首个 attachCubeFace 选面
    _mip = 0;
    _cubeIsDepth = true;
    return true;
}

void* DX11RenderTarget::colorTexture() {
    if (!_colors.empty()) return _colors[0]->handle();
    if (!_msaaColors.empty()) return _msaaColors[0]->handle();
    return nullptr;
}

ITexture2D* DX11RenderTarget::colorTexture2D(int attachment) {
    if (attachment < 0) return nullptr;
    const size_t idx = static_cast<size_t>(attachment);
    if (idx < _colors.size()) return _colors[idx].get();
    const size_t msaaIdx = idx - _colors.size();
    if (msaaIdx < _msaaColors.size()) return _msaaColors[msaaIdx].get();
    return nullptr;
}

ITexture2D* DX11RenderTarget::depthTexture2D() { return _depth.get(); }

bool DX11RenderTarget::resolveTo(IRenderTarget&) {
    return true;   // MSAA resolve 由 blitFramebuffer 的 ResolveSubresource 承担（同 VK/DX12 no-op）
}

void* DX11RenderTarget::handle() { return colorTexture(); }

void DX11RenderTarget::release() {
    _depth.reset();
    _msaaColors.clear();
    _colors.clear();
    for (auto& v : _rtvs) v.Reset();
    _rtvs.clear();
    _dsv.Reset();
    _cube = nullptr;
    _face = -1;
    _mip = 0;
    _cubeIsDepth = false;
    _dsvFormat = DXGI_FORMAT_UNKNOWN;
    _width = 0;
    _height = 0;
    _samples = 1;
    _valid = false;
}

uint32_t DX11RenderTarget::colorCount() const {
    if (!_valid) return 0;
    if (_cube && _cubeIsDepth && _face >= 0) return 0;   // 纯深度 pass 合法
    if (_cube && !_cubeIsDepth && _face >= 0) return 1;  // cube 面即唯一颜色目标（IBL capture）
    return static_cast<uint32_t>(_colors.size() + _msaaColors.size());
}

UINT DX11RenderTarget::FillRtvs(ID3D11RenderTargetView** out, UINT maxCount) {
    if (!_valid) return 0;
    UINT n = 0;
    if (_cube && !_cubeIsDepth && _face >= 0) {
        if (n < maxCount) out[n++] = _cube->rtvFace(_face, _mip);
        return n;
    }
    for (auto& v : _rtvs) {
        if (n >= maxCount) break;
        out[n++] = v.Get();
    }
    return n;
}

ID3D11DepthStencilView* DX11RenderTarget::ActiveDsv() {
    if (!_valid) return nullptr;
    if (_cube && _cubeIsDepth && _face >= 0) return _cube->dsvFace(_face);
    return _dsv.Get();
}

bool DX11RenderTarget::hasDepthAttachment() const {
    if (_cube && _cubeIsDepth) return true;
    return _dsv.Get() != nullptr;
}

// viewport 兜底尺寸：cube 面 attach 且 mip>0 时取该级尺寸（prefilter 式逐级捕获）
void DX11RenderTarget::renderDims(int& w, int& h) const {
    w = _width;
    h = _height;
    if (_cube && _face >= 0 && _mip > 0) {
        w = std::max(1, _width >> _mip);
        h = std::max(1, _height >> _mip);
    }
}

bool DX11RenderTarget::depthHasStencil() const {
    DXGI_FORMAT fmt = _dsvFormat;
    if (_cube && _cubeIsDepth) fmt = _cube ? _cube->dsvFormat() : DXGI_FORMAT_UNKNOWN;
    return fmt == DXGI_FORMAT_D24_UNORM_S8_UINT;
}

void DX11RenderTarget::ClearAll(ID3D11DeviceContext* ctx, const std::array<float, 4>& cc) {
    if (!ctx || !_valid) return;
    ID3D11RenderTargetView* rtvs[8];
    const UINT n = FillRtvs(rtvs, 8);
    for (UINT i = 0; i < n; ++i) {
        ctx->ClearRenderTargetView(rtvs[i], cc.data());
    }
    ID3D11DepthStencilView* dsv = ActiveDsv();
    if (dsv) {
        UINT flags = D3D11_CLEAR_DEPTH;
        if (depthHasStencil()) flags |= D3D11_CLEAR_STENCIL;
        ctx->ClearDepthStencilView(dsv, flags, 1.0f, 0);
    }
}

} // namespace rhi
