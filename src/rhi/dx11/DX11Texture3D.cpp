#include "rhi/dx11/DX11Backend.hpp"
#include "base/Log.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace rhi {

namespace {

// 标准链长 = floor(log2(max(w,h))) + 1（与 GL/VK/DX12 分配一致）
UINT ComputeMipLevels(int w, int h) {
    int m = std::max(w, h);
    UINT levels = 1;
    while (m > 1) { m >>= 1; ++levels; }
    return levels;
}

// CPU 上传仅支持 RGBA8 族（LoadCube 的 4 通道强制加载已保证）；ch==3 兜底补 alpha
// （对照 DXTexture3D::ExpandRgba8 逐分支一致）
bool ExpandRgba8(const void* src, int w, int h, int ch, std::vector<uint8_t>& out) {
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    out.resize(n * 4);
    if (ch == 4) {
        std::memcpy(out.data(), src, n * 4);
    } else if (ch == 3) {
        const uint8_t* s = static_cast<const uint8_t*>(src);
        for (size_t i = 0; i < n; ++i) {
            out[i * 4 + 0] = s[i * 3 + 0];
            out[i * 4 + 1] = s[i * 3 + 1];
            out[i * 4 + 2] = s[i * 3 + 2];
            out[i * 4 + 3] = 255;
        }
    } else {
        LOGE("[DX11] unsupported channel count {} for texture upload", ch);
        return false;
    }
    return true;
}

} // namespace

DX11Texture3D::DX11Texture3D(ID3D11Device* device, ID3D11DeviceContext* context)
    : _device(device), _context(context) {}

DX11Texture3D::~DX11Texture3D() { release(); }

bool DX11Texture3D::init(const TextureDataView3D& data) {
    release();
    if (!_device || !data.data || data.width <= 0 || data.height <= 0) return false;
    _depthSlices = data.depth > 0 ? data.depth : 1;

    // 旧签名按 RGBA8 承载（同 VK e3D/DX12 init 路径的存储布局）；z 切片行主序
    // 连续排布，等价于 height*depth 行的 2D 图展开
    std::vector<uint8_t> expanded;
    if (!ExpandRgba8(data.data, data.width, data.height * _depthSlices,
                     data.channels > 0 ? data.channels : 4, expanded)) {
        return false;
    }
    _format = DXGI_FORMAT_R8G8B8A8_UNORM;
    _srvFormat = _format;
    _width = data.width;
    _height = data.height;
    _mipLevels = 1;
    _cube = false;

    D3D11_TEXTURE3D_DESC td{};
    td.Width = static_cast<UINT>(_width);
    td.Height = static_cast<UINT>(_height);
    td.Depth = static_cast<UINT>(_depthSlices);
    td.MipLevels = 1;
    td.Format = _format;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = 0;
    td.MiscFlags = 0;
    DX11_CHECK(_device->CreateTexture3D(&td, nullptr, &_texture3d), "create texture3d");
    if (!_texture3d.Get()) return false;

    D3D11_TEXTURE3D_DESC sd = td;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Dx11ComPtr<ID3D11Texture3D> staging;
    DX11_CHECK(_device->CreateTexture3D(&sd, nullptr, &staging), "create texture3d staging");
    if (!staging.Get()) return false;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = _context->Map(staging.Get(), 0, D3D11_MAP_WRITE, 0, &mapped);
    if (FAILED(hr) || !mapped.pData) {
        LOGE("[DX11] map texture3d staging failed hr=0x{:08X}", static_cast<uint32_t>(hr));
        return false;
    }
    const size_t rowBytes = static_cast<size_t>(_width) * 4;
    const int totalRows = _height * _depthSlices;
    for (int row = 0; row < totalRows; ++row) {
        std::memcpy(static_cast<uint8_t*>(mapped.pData) +
                        static_cast<size_t>(row) * mapped.RowPitch,
                    expanded.data() + rowBytes * row, rowBytes);
    }
    _context->Unmap(staging.Get(), 0);

    D3D11_BOX box{0, 0, 0, static_cast<UINT>(_width), static_cast<UINT>(_height),
                  static_cast<UINT>(_depthSlices)};
    _context->CopySubresourceRegion(_texture3d.Get(), 0, 0, 0, 0, staging.Get(), 0, &box);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format = _srvFormat;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    srvd.Texture3D.MostDetailedMip = 0;
    srvd.Texture3D.MipLevels = 1;
    DX11_CHECK(_device->CreateShaderResourceView(_texture3d.Get(), &srvd, &_srv),
               "create texture3d srv");
    if (!_srv.Get()) return false;

    _valid = true;
    return true;
}

bool DX11Texture3D::initCube(const TextureDesc& desc, const TextureDataView2D* faces) {
    release();
    if (!_device || !faces) return false;
    for (int f = 0; f < 6; ++f) {
        if (!faces[f].data || faces[f].width <= 0 || faces[f].height <= 0) return false;
    }

    // CPU 上传仅支持 RGBA8 存储（RGB8 以 RGBA8 承载，展开兜底同 DX12）
    if (desc.format != TextureFormat::RGBA8 && desc.format != TextureFormat::RGB8) {
        LOGE("[DX11] initCube CPU upload only supports RGBA8, got {}", static_cast<int>(desc.format));
        return false;
    }
    _params = desc;
    _cube = true;
    _depth = false;
    _format = DXGI_FORMAT_R8G8B8A8_UNORM;
    _srvFormat = _format;
    _rtvFormat = _format;   // 颜色 cube 可再经 attachCubeFace 渲染（RTV 视图同格式）
    _width = faces[0].width;
    _height = faces[0].height;
    _depthSlices = 6;
    _mipLevels = desc.generateMipmap ? ComputeMipLevels(_width, _height) : 1;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(_width);
    td.Height = static_cast<UINT>(_height);
    td.MipLevels = static_cast<UINT>(_mipLevels);
    td.ArraySize = 6;
    td.Format = _format;
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    // BIND_RT 无条件携带（Task 6 M-2）：attachCubeFace 的逐面 RTV 要求资源带
    // RENDER_TARGET 绑定，仅 generateMipmap=true 时才有会使无 mip cube（如未来
    // IBL envCubemap CPU 上传路径）挂接即败；颜色 cube 携带 RT 无副作用。
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
    if (_mipLevels > 1) {
        // GenerateMips 硬性要求：资源带 MISC_GENERATE_MIPS（SRV+RT 已齐备）
        td.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
        _mipsBindable = true;
    }
    DX11_CHECK(_device->CreateTexture2D(&td, nullptr, &_texture), "create cube texture");
    if (!_texture.Get()) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format = _srvFormat;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvd.TextureCube.MostDetailedMip = 0;
    srvd.TextureCube.MipLevels = static_cast<UINT>(_mipLevels);
    DX11_CHECK(_device->CreateShaderResourceView(_texture.Get(), &srvd, &_srv),
               "create cube srv");
    if (!_srv.Get()) return false;

    // 逐面 STAGING → CopySubresourceRegion（子资源索引 = face*mipLevels 即面 mip0）
    const size_t rowBytes = static_cast<size_t>(_width) * 4;
    for (int f = 0; f < 6; ++f) {
        std::vector<uint8_t> expanded;
        if (!ExpandRgba8(faces[f].data, faces[f].width, faces[f].height,
                         faces[f].channels > 0 ? faces[f].channels : 4, expanded)) {
            continue;   // 已 LOGE；该面不上传（内容为未定义）
        }
        D3D11_TEXTURE2D_DESC sd = td;
        sd.MipLevels = 1;
        sd.ArraySize = 1;
        sd.Usage = D3D11_USAGE_STAGING;
        sd.BindFlags = 0;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        sd.MiscFlags = 0;
        Dx11ComPtr<ID3D11Texture2D> staging;
        DX11_CHECK(_device->CreateTexture2D(&sd, nullptr, &staging), "create cube face staging");
        if (!staging.Get()) continue;
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = _context->Map(staging.Get(), 0, D3D11_MAP_WRITE, 0, &mapped);
        if (FAILED(hr) || !mapped.pData) {
            LOGE("[DX11] map cube face staging failed hr=0x{:08X}", static_cast<uint32_t>(hr));
            continue;
        }
        for (int row = 0; row < _height; ++row) {
            std::memcpy(static_cast<uint8_t*>(mapped.pData) +
                            static_cast<size_t>(row) * mapped.RowPitch,
                        expanded.data() + rowBytes * row, rowBytes);
        }
        _context->Unmap(staging.Get(), 0);
        _context->CopySubresourceRegion(_texture.Get(),
                                        static_cast<UINT>(f) * _mipLevels,
                                        0, 0, 0, staging.Get(), 0, nullptr);
    }

    // Task 5：先置位再生成（genCubeMipmaps 以 valid 为门禁，DX12 同教训）——
    // 内部经注入的 blit 能力逐面 Gather 盒平均降采样，能力缺失回退 GenerateMips
    _valid = true;
    if (_mipLevels > 1) {
        genCubeMipmaps();
    }
    return true;
}

bool DX11Texture3D::createEmpty(const TextureDesc& desc, int width, int height) {
    release();
    if (!_device || width <= 0 || height <= 0) return false;

    _params = desc;
    _cube = true;
    _depth = desc.format == TextureFormat::Depth32F ||
             desc.format == TextureFormat::Depth24Stencil8;
    // 深度资源必须用 TYPELESS 族格式（DSV+SRV 双绑定的 D3D11 规则），视图阶段取
    // typed 格式（同 DX11Texture2D::createEmpty 口径）
    if (desc.format == TextureFormat::Depth32F) {
        _format = DXGI_FORMAT_R32_TYPELESS;
        _srvFormat = DXGI_FORMAT_R32_FLOAT;
        _dsvFormat = DXGI_FORMAT_D32_FLOAT;
    } else if (desc.format == TextureFormat::Depth24Stencil8) {
        _format = DXGI_FORMAT_R24G8_TYPELESS;
        _srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        _dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    } else {
        _format = Dx11FormatOf(desc.format);
        _srvFormat = _format;
        _rtvFormat = _format;
    }
    _width = width;
    _height = height;
    _depthSlices = 6;
    _mipLevels = desc.generateMipmap ? ComputeMipLevels(width, height) : 1;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(width);
    td.Height = static_cast<UINT>(height);
    td.MipLevels = static_cast<UINT>(_mipLevels);
    td.ArraySize = 6;
    td.Format = _format;
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    // BIND 标志按用途：深度=DEPTH_STENCIL(+SRV 供阴影 cube 采样)；颜色=RT(+SRV，
    // attachCubeFace 渲入面的 RTV 要求，如后续 IBL 捕获)；MISC_TEXTURECUBE 是
    // TEXTURECUBE 维度 SRV 的硬性前置
    td.BindFlags = _depth ? (D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE)
                          : (D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    td.CPUAccessFlags = 0;
    td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
    if (!_depth && _mipLevels > 1) {
        td.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
        _mipsBindable = true;
    }
    DX11_CHECK(_device->CreateTexture2D(&td, nullptr, &_texture), "create empty cube");
    if (!_texture.Get()) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format = _srvFormat;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvd.TextureCube.MostDetailedMip = 0;
    srvd.TextureCube.MipLevels = static_cast<UINT>(_mipLevels);
    DX11_CHECK(_device->CreateShaderResourceView(_texture.Get(), &srvd, &_srv),
               "create empty cube srv");
    if (!_srv.Get()) return false;

    _valid = true;
    return true;
}

ID3D11DepthStencilView* DX11Texture3D::dsvFace(int face) {
    if (!_valid || !_depth || !_texture.Get() || face < 0 || face > 5) return nullptr;
    if (!_faceDsv[0].Get()) {
        for (int f = 0; f < 6; ++f) {
            D3D11_DEPTH_STENCIL_VIEW_DESC dd{};
            dd.Format = _dsvFormat;
            dd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dd.Texture2DArray.MipSlice = 0;
            dd.Texture2DArray.FirstArraySlice = static_cast<UINT>(f);
            dd.Texture2DArray.ArraySize = 1;
            DX11_CHECK(_device->CreateDepthStencilView(_texture.Get(), &dd, &_faceDsv[f]),
                       "create cube face dsv");
        }
    }
    return _faceDsv[face].Get();
}

ID3D11RenderTargetView* DX11Texture3D::rtvFace(int face, int mip) {
    if (!_valid || _depth || !_texture.Get() || face < 0 || face > 5 ||
        mip < 0 || static_cast<UINT>(mip) >= _mipLevels) {
        return nullptr;
    }
    if (_faceRtv.empty()) {
        _faceRtv.resize(6u * _mipLevels);
        for (UINT m = 0; m < _mipLevels; ++m) {
            for (UINT f = 0; f < 6; ++f) {
                D3D11_RENDER_TARGET_VIEW_DESC rd{};
                rd.Format = _rtvFormat;
                rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rd.Texture2DArray.MipSlice = m;
                rd.Texture2DArray.FirstArraySlice = f;
                rd.Texture2DArray.ArraySize = 1;
                DX11_CHECK(_device->CreateRenderTargetView(
                               _texture.Get(), &rd, &_faceRtv[f * _mipLevels + m]),
                           "create cube face rtv");
            }
        }
    }
    return _faceRtv[static_cast<size_t>(face) * _mipLevels + static_cast<UINT>(mip)].Get();
}

void DX11Texture3D::genCubeMipmaps() {
    if (!_valid || !_cube || _depth || _mipLevels <= 1) return;
    if (!_mipsBindable) {
        LOGW("[DX11] genCubeMipmaps: resource lacks RT+GENERATE_MIPS binds; clamping to mip0");
        _mipLevels = 1;
        return;
    }
    // Task 5：注入的 blit 能力走逐面 Gather 盒平均降采样（mipdown_array.frag，
    // 源视图 TEXTURE2DARRAY 单面单级——对齐 DX12 口径）；能力缺失回退 D3D11
    // 内建 GenerateMips 兜底（TEXTURECUBE SRV 全链一次完成）
    if (_blitCtx && _blitCtx->MipdownCube(this)) {
        return;
    }
    LOGW("[DX11] mipdown blit unavailable; falling back to GenerateMips");
    _context->GenerateMips(_srv.Get());
}

void* DX11Texture3D::handle() {
    return _texture.Get() ? static_cast<void*>(_texture.Get())
                          : static_cast<void*>(_texture3d.Get());
}

void DX11Texture3D::release() {
    for (auto& d : _faceDsv) d.Reset();
    _faceRtv.clear();
    _texture.Reset();
    _texture3d.Reset();
    _srv.Reset();
    _format = DXGI_FORMAT_UNKNOWN;
    _srvFormat = DXGI_FORMAT_UNKNOWN;
    _dsvFormat = DXGI_FORMAT_UNKNOWN;
    _rtvFormat = DXGI_FORMAT_UNKNOWN;
    _mipLevels = 1;
    _width = 0;
    _height = 0;
    _depthSlices = 0;
    _cube = false;
    _depth = false;
    _mipsBindable = false;
    _valid = false;
}

} // namespace rhi
