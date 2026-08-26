#include "rhi/dx11/DX11Backend.hpp"
#include "base/Log.hpp"
#include <algorithm>
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

// float32 → half16（对齐 VKTexture2D/DXTexture2D 的转换）
uint16_t FloatToHalf(float f) {
    const uint32_t x = *reinterpret_cast<uint32_t*>(&f);
    const uint32_t sign = (x >> 16) & 0x8000;
    int32_t exp = static_cast<int32_t>((x >> 23) & 0xff) - 127 + 15;
    uint32_t man = x & 0x7fffff;
    if (exp >= 31) return static_cast<uint16_t>(sign | 0x7c00);
    if (exp <= 0) {
        if (exp < -10) return static_cast<uint16_t>(sign);
        man |= 0x800000;
        const uint32_t shift = static_cast<uint32_t>(14 - exp);
        return static_cast<uint16_t>(sign | (man >> shift));
    }
    return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exp) << 10) | (man >> 13));
}

// 把源数据展开为存储布局的紧凑行主序缓冲（对照 DXTexture2D.cpp 同名函数逐分支一致：
// RGB8/RGBA8→RGBA8 补 255；half 浮点族补 1.0/0；RGBA32F 补 alpha=1）。
bool ExpandToStorage(TextureFormat fmt, const void* src, int w, int h, int ch,
                     std::vector<uint8_t>& out, UINT& texelOut) {
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    switch (fmt) {
        case TextureFormat::RGBA8:
        case TextureFormat::RGB8: {
            texelOut = 4;
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
                LOGE("[DX11] unsupported channel count {} for RGBA8 upload", ch);
                return false;
            }
            return true;
        }
        case TextureFormat::RGB16F:
        case TextureFormat::RGBA16F: {
            texelOut = 8;
            out.resize(n * 8);
            if (ch != 3 && ch != 4) {
                LOGE("[DX11] unsupported channel count {} for half-float upload", ch);
                return false;
            }
            const float* s = static_cast<const float*>(src);
            for (size_t i = 0; i < n; ++i) {
                auto* d = reinterpret_cast<uint16_t*>(out.data() + i * 8);
                d[0] = FloatToHalf(s[i * ch + 0]);
                d[1] = FloatToHalf(s[i * ch + 1]);
                d[2] = FloatToHalf(s[i * ch + 2]);
                d[3] = ch == 4 ? FloatToHalf(s[i * ch + 3]) : static_cast<uint16_t>(0x3c00);
            }
            return true;
        }
        case TextureFormat::RG16F: {
            texelOut = 4;
            if (ch < 2) {
                LOGE("[DX11] unsupported channel count {} for RG16F upload", ch);
                return false;
            }
            out.resize(n * 4);
            const float* s = static_cast<const float*>(src);
            for (size_t i = 0; i < n; ++i) {
                auto* d = reinterpret_cast<uint16_t*>(out.data() + i * 4);
                d[0] = FloatToHalf(s[i * ch + 0]);
                d[1] = FloatToHalf(s[i * ch + 1]);
            }
            return true;
        }
        case TextureFormat::R32F: {
            texelOut = 4;
            if (ch != 1) {
                LOGE("[DX11] unsupported channel count {} for R32F upload", ch);
                return false;
            }
            out.resize(n * 4);
            std::memcpy(out.data(), src, n * 4);
            return true;
        }
        case TextureFormat::RGBA32F: {
            texelOut = 16;
            out.resize(n * 16);
            if (ch == 4) {
                std::memcpy(out.data(), src, n * 16);
            } else if (ch == 3) {
                const float* s = static_cast<const float*>(src);
                for (size_t i = 0; i < n; ++i) {
                    float* d = reinterpret_cast<float*>(out.data() + i * 16);
                    d[0] = s[i * 3 + 0];
                    d[1] = s[i * 3 + 1];
                    d[2] = s[i * 3 + 2];
                    d[3] = 1.0f;
                }
            } else {
                LOGE("[DX11] unsupported channel count {} for RGBA32F upload", ch);
                return false;
            }
            return true;
        }
        default:
            LOGE("[DX11] CPU upload not supported for format {}", static_cast<int>(fmt));
            return false;
    }
}

} // namespace

DX11Texture2D::DX11Texture2D(ID3D11Device* device, ID3D11DeviceContext* context)
    : _device(device), _context(context) {}

DX11Texture2D::~DX11Texture2D() { release(); }

bool DX11Texture2D::init(const TextureDataView2D& data) {
    return init(TextureDesc{}, data);
}

bool DX11Texture2D::init(const TextureDesc& desc, const TextureDataView2D& data) {
    release();
    if (!_device || !data.data || data.width <= 0 || data.height <= 0) return false;
    // 深度格式走 createEmpty（渲染目标），CPU 上传路径不支持
    if (desc.format == TextureFormat::Depth32F ||
        desc.format == TextureFormat::Depth24Stencil8) {
        LOGW("[DX11] depth texture upload unsupported; use createEmpty");
        return false;
    }

    _format = Dx11FormatOf(desc.format);   // typed 直传（上传路径无 TYPELESS 需求）
    _srvFormat = _format;
    _width = data.width;
    _height = data.height;
    _params = desc;

    // generateMipmap=true 才分配 mip 链并降采样（同 VKTexture2D/DXTexture2D）。
    // 非 filterable 格式（float32 族）无法 GenerateMips：钳 mip0 兜底
    // （对齐 DX12 "mipgen 不可用钳 mip0" 的降级语义）
    _mipLevels = desc.generateMipmap ? ComputeMipLevels(_width, _height) : 1;
    if (_mipLevels > 1 &&
        (desc.format == TextureFormat::RGBA32F || desc.format == TextureFormat::R32F)) {
        LOGW("[DX11] GenerateMips unsupported for format {}; clamping to mip0",
             static_cast<int>(desc.format));
        _mipLevels = 1;
    }

    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(_width);
    td.Height = static_cast<UINT>(_height);
    td.MipLevels = static_cast<UINT>(_mipLevels);
    td.ArraySize = 1;
    td.Format = _format;
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    // GenerateMips 硬性要求：资源带 MISC_GENERATE_MIPS 且同时绑定 SRV+RT 标志
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.MiscFlags = 0;
    if (_mipLevels > 1) {
        td.BindFlags |= D3D11_BIND_RENDER_TARGET;
        td.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }
    DX11_CHECK(_device->CreateTexture2D(&td, nullptr, &_texture), "create texture");
    if (!_texture.Get()) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = _srvFormat;
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sd.Texture2D.MostDetailedMip = 0;
    sd.Texture2D.MipLevels = static_cast<UINT>(_mipLevels);   // 全 mip 链（隐式 LOD 可用）
    DX11_CHECK(_device->CreateShaderResourceView(_texture.Get(), &sd, &_srv),
               "create shader resource view");
    if (!_srv.Get()) return false;

    if (!uploadAndGenMips(desc, data)) {
        return false;
    }
    // 上传路径与 createEmpty 同样须置 valid（Model::loadMaterialTextures 以
    // valid() 判定成败，漏置会让全部模型纹理被误报加载失败——DX12 同教训）
    _valid = true;
    return true;
}

// STAGING 纹理 Map 行拷贝 → CopySubresourceRegion 写 mip0 →（可选）GenerateMips。
// brief 指定路径：CPU 上传 staging+CopySubresourceRegion。
bool DX11Texture2D::uploadAndGenMips(const TextureDesc&, const TextureDataView2D& data) {
    if (!_device || !_context || !_texture.Get() || !_srv.Get()) return false;
    UINT texel = 4;
    std::vector<uint8_t> expanded;
    if (!ExpandToStorage(_params.format, data.data, _width, _height, data.channels,
                         expanded, texel)) {
        return false;
    }

    D3D11_TEXTURE2D_DESC sd{};
    sd.Width = static_cast<UINT>(_width);
    sd.Height = static_cast<UINT>(_height);
    sd.MipLevels = 1;
    sd.ArraySize = 1;
    sd.Format = _format;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    sd.MiscFlags = 0;
    Dx11ComPtr<ID3D11Texture2D> staging;
    DX11_CHECK(_device->CreateTexture2D(&sd, nullptr, &staging), "create upload staging texture");
    if (!staging.Get()) return false;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = _context->Map(staging.Get(), 0, D3D11_MAP_WRITE, 0, &mapped);
    if (FAILED(hr) || !mapped.pData) {
        LOGE("[DX11] map staging failed hr=0x{:08X}", static_cast<uint32_t>(hr));
        return false;
    }
    const size_t rowBytes = static_cast<size_t>(_width) * texel;
    for (int row = 0; row < _height; ++row) {
        std::memcpy(static_cast<uint8_t*>(mapped.pData) +
                        static_cast<size_t>(row) * mapped.RowPitch,
                    expanded.data() + rowBytes * row, rowBytes);
    }
    _context->Unmap(staging.Get(), 0);

    D3D11_BOX box{0, 0, 0, static_cast<UINT>(_width), static_cast<UINT>(_height), 1};
    _context->CopySubresourceRegion(_texture.Get(), 0, 0, 0, 0, staging.Get(), 0, &box);

    if (_mipLevels > 1) {
        // Task 5：注入的 blit 能力走 Gather 角点盒平均降采样（mipdown.frag，对齐
        // GL generateMipmap/vkCmdBlitImage 的 2:1 盒式语义）；能力缺失回退 D3D11
        // 内建 GenerateMips 兜底（语义近似但驱动相关）
        if (_blitCtx && _blitCtx->Mipdown2D(this)) {
            // 手动降采样完成
        } else {
            LOGW("[DX11] mipdown blit unavailable; falling back to GenerateMips");
            _context->GenerateMips(_srv.Get());
        }
    }
    return true;
}

bool DX11Texture2D::createEmpty(const TextureDesc& desc, int width, int height) {
    release();
    if (!_device || width <= 0 || height <= 0) return false;

    _params = desc;
    _width = width;
    _height = height;
    _mipLevels = 1;   // 与 VK/DX12 createEmpty 一致：不生成 mip 链

    const bool depth = desc.format == TextureFormat::Depth32F ||
                       desc.format == TextureFormat::Depth24Stencil8;
    // 深度资源必须用 TYPELESS 族格式（DSV+SRV 双绑定的 D3D11 规则），视图阶段取
    // typed 格式：DSV=D32_FLOAT/D24_UNORM_S8_UINT，SRV=R32_FLOAT/R24_UNORM_X8_TYPELESS
    // （srvFormat 口径同 DXTexture2D::createEmpty）
    DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
    if (desc.format == TextureFormat::Depth32F) {
        _format = DXGI_FORMAT_R32_TYPELESS;
        _srvFormat = DXGI_FORMAT_R32_FLOAT;
        dsvFormat = DXGI_FORMAT_D32_FLOAT;
    } else if (desc.format == TextureFormat::Depth24Stencil8) {
        _format = DXGI_FORMAT_R24G8_TYPELESS;
        _srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    } else {
        _format = Dx11FormatOf(desc.format);
        _srvFormat = _format;
    }

    const bool msaa = desc.multisample && desc.samples > 1;
    _msaa = msaa;
    UINT sampleCount = 1;
    if (msaa) {
        // MSAA 4X/8X 档位按 samples 选择，CheckMultisampleQualityLevels 校验后回落
        for (UINT want : {desc.samples >= 8 ? 8u : 4u, 4u}) {
            UINT quality = 0;
            if (SUCCEEDED(_device->CheckMultisampleQualityLevels(
                    depth ? dsvFormat : _format, want, &quality)) &&
                quality > 0) {
                sampleCount = want;
                break;
            }
        }
        if (sampleCount == 1) LOGW("[DX11] MSAA {}x unsupported, falling back to x1", desc.samples);
    }

    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(width);
    td.Height = static_cast<UINT>(height);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = _format;
    td.SampleDesc.Count = sampleCount;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    // BIND 标志按用途：深度=DEPTH_STENCIL(+SRV 供后续阴影采样)；颜色=RT(+SRV)；
    // MSAA 颜色仅 RT（不可采样，同 DX12 DENY_SHADER_RESOURCE 语义由守卫保证）
    td.BindFlags = depth ? (D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE)
                         : (D3D11_BIND_RENDER_TARGET | (msaa ? 0 : D3D11_BIND_SHADER_RESOURCE));
    td.CPUAccessFlags = 0;
    td.MiscFlags = 0;
    DX11_CHECK(_device->CreateTexture2D(&td, nullptr, &_texture), "create empty texture");
    if (!_texture.Get()) return false;

    if (!msaa) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
        srvd.Format = _srvFormat;
        srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvd.Texture2D.MostDetailedMip = 0;
        srvd.Texture2D.MipLevels = 1;
        DX11_CHECK(_device->CreateShaderResourceView(_texture.Get(), &srvd, &_srv),
                   "create empty texture srv");
        if (!_srv.Get()) return false;
    }

    _valid = true;
    return true;
}

void DX11Texture2D::setBorderColor(const float bc[4]) {
    if (!bc) return;
    _borderColor = {bc[0], bc[1], bc[2], bc[3]};
}

void DX11Texture2D::release() {
    _texture.Reset();
    _srv.Reset();
    _format = DXGI_FORMAT_UNKNOWN;
    _srvFormat = DXGI_FORMAT_UNKNOWN;
    _mipLevels = 1;
    _width = 0;
    _height = 0;
    _msaa = false;
    _valid = false;
}

} // namespace rhi
