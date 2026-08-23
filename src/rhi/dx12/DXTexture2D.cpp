#include "rhi/dx12/DXTexture2D.hpp"
#include "rhi/dx12/DXFormat.hpp"
#include "base/Log.hpp"
#include <algorithm>
#include <cstring>
#include <vector>

namespace rhi {

namespace {

size_t AlignUp(size_t v, size_t a) { return (v + a - 1) / a * a; }

// 标准链长 = floor(log2(max(w,h))) + 1（与 GL/VK 分配一致）
UINT ComputeMipLevels(int w, int h) {
    int m = std::max(w, h);
    UINT levels = 1;
    while (m > 1) { m >>= 1; ++levels; }
    return levels;
}

// float32 → half16（对齐 VKTexture2D::init 的转换）
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

// 把源数据展开为存储布局的紧凑行主序缓冲。3 通道源按目标分量数补齐
// （RGB8→RGBA8 补 255；RGB16F/RGBA32F 源为 stbi_loadf 的 float 布局）。
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
                LOGE("[DX12] unsupported channel count {} for RGBA8 upload", ch);
                return false;
            }
            return true;
        }
        case TextureFormat::RGB16F:
        case TextureFormat::RGBA16F: {
            texelOut = 8;
            out.resize(n * 8);
            if (ch != 3 && ch != 4) {
                LOGE("[DX12] unsupported channel count {} for half-float upload", ch);
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
                LOGE("[DX12] unsupported channel count {} for RG16F upload", ch);
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
                LOGE("[DX12] unsupported channel count {} for R32F upload", ch);
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
                LOGE("[DX12] unsupported channel count {} for RGBA32F upload", ch);
                return false;
            }
            return true;
        }
        default:
            LOGE("[DX12] CPU upload not supported for format {}", static_cast<int>(fmt));
            return false;
    }
}

} // namespace

DXTexture2D::DXTexture2D(ID3D12Device* device, ID3D12CommandQueue* queue,
                         ID3D12CommandAllocator* uploadAlloc, ID3D12Fence* uploadFence,
                         HANDLE fenceEvent)
    : _device(device)
    , _queue(queue)
    , _uploadAlloc(uploadAlloc)
    , _uploadFence(uploadFence)
    , _fenceEvent(fenceEvent) {}

DXTexture2D::~DXTexture2D() { release(); }

bool DXTexture2D::init(const TextureDataView2D& data) {
    return init(TextureDesc{}, data);
}

bool DXTexture2D::init(const TextureDesc& desc, const TextureDataView2D& data) {
    release();
    if (!_device || !data.data || data.width <= 0 || data.height <= 0) return false;
    // 深度格式走 createEmpty（渲染目标），CPU 上传路径不支持
    if (desc.format == TextureFormat::Depth32F ||
        desc.format == TextureFormat::Depth24Stencil8) {
        LOGW("[DX12] depth texture upload unsupported; use createEmpty");
        return false;
    }

    _format = DXFormatOf(desc.format);
    _srvFormat = _format;
    _width = data.width;
    _height = data.height;
    _params = desc;

    // generateMipmap=true 才分配 mip 链并降采样（同 VKTexture2D）
    _mipLevels = desc.generateMipmap ? ComputeMipLevels(_width, _height) : 1;

    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (_mipLevels > 1) flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;  // 渲到 mip 层 RTV 降采样
    if (!createResource(static_cast<UINT>(_width), static_cast<UINT>(_height), flags, 1)) {
        return false;
    }
    if (!uploadAndGenMips(desc, data)) {
        return false;
    }
    // 上传路径与 createEmpty 同样须置 valid（Model::loadMaterialTextures 以
    // valid() 判定成败，漏置会让全部模型纹理被误报加载失败）
    _valid = true;
    return true;
}

bool DXTexture2D::createEmpty(const TextureDesc& desc, int width, int height) {
    release();
    if (!_device || width <= 0 || height <= 0) return false;

    _format = DXFormatOf(desc.format);
    _width = width;
    _height = height;
    _mipLevels = 1;   // 与 VKTexture2D::createEmpty 一致：不生成 mip 链
    _params = desc;

    const bool depth = desc.format == TextureFormat::Depth32F ||
                       desc.format == TextureFormat::Depth24Stencil8;
    // SRV 视图 typed 格式：TYPELESS 资源建 SRV 时必须给具体格式
    // （Depth32F→R32_FLOAT 阴影采样 / D24S8→R24_UNORM_X8_TYPELESS）
    if (desc.format == TextureFormat::Depth32F) {
        _srvFormat = DXGI_FORMAT_R32_FLOAT;
    } else if (desc.format == TextureFormat::Depth24Stencil8) {
        _srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    } else {
        _srvFormat = _format;
    }

    const bool msaa = desc.multisample && desc.samples > 1;
    _msaa = msaa;
    UINT sampleCount = 1;
    if (msaa) {
        // MSAA 4X/8X 档位按 samples 选择，CheckFeatureSupport 校验后回落
        const DXGI_FORMAT checkFmt = desc.format == TextureFormat::Depth32F
                                         ? DXGI_FORMAT_D32_FLOAT
                                         : (desc.format == TextureFormat::Depth24Stencil8
                                                ? DXGI_FORMAT_D24_UNORM_S8_UINT
                                                : _format);
        for (UINT want : {desc.samples >= 8 ? 8u : 4u, 4u}) {
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS q{};
            q.Format = checkFmt;
            q.SampleCount = want;
            if (SUCCEEDED(_device->CheckFeatureSupport(
                    D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &q, sizeof(q))) &&
                q.NumQualityLevels > 0) {
                sampleCount = want;
                break;
            }
        }
        if (sampleCount == 1) LOGW("[DX12] MSAA {}x unsupported, falling back to x1", desc.samples);
    }

    D3D12_RESOURCE_FLAGS flags = depth ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
                                       : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    // 注意：D3D12 中 DENY_SHADER_RESOURCE 仅允许与 ALLOW_DEPTH_STENCIL 等组合，
    // 与 ALLOW_RENDER_TARGET 同用会 E_INVALIDARG（调试层 #719）；MSAA 颜色资源
    // 只带 ALLOW_RENDER_TARGET，SRV 由 bind 路径的 isMsaa 守卫保证不创建

    if (!createResource(static_cast<UINT>(width), static_cast<UINT>(height),
                        flags, sampleCount)) {
        return false;
    }
    // 资源以 COPY_DEST 创建，无上传数据时立即转入常驻态：深度=DEPTH_WRITE 供渲染
    // 目标；非 MSAA 颜色=PSHR 供采样；MSAA 颜色=RENDER_TARGET（渲染路径管理）
    const D3D12_RESOURCE_STATES finalState =
        depth ? D3D12_RESOURCE_STATE_DEPTH_WRITE
              : (msaa ? D3D12_RESOURCE_STATE_RENDER_TARGET
                      : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if (!executeOneShot([this, finalState](ID3D12GraphicsCommandList* cmd) {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = _resource.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = finalState;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            cmd->ResourceBarrier(1, &barrier);
        })) {
        return false;
    }
    _valid = true;
    return true;
}

bool DXTexture2D::createResource(UINT width, UINT height,
                                 D3D12_RESOURCE_FLAGS flags, UINT sampleCount) {
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = width;
    rd.Height = static_cast<UINT>(height);
    rd.DepthOrArraySize = 1;
    rd.MipLevels = static_cast<UINT16>(_mipLevels);
    rd.Format = _format;
    rd.SampleDesc.Count = sampleCount > 0 ? sampleCount : 1;
    rd.SampleDesc.Quality = 0;
    rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    rd.Flags = flags;
    // 统一以 COPY_DEST 创建；上传路径拷贝后转 PSHR，createEmpty 成功后立即
    // 转常驻态（颜色=PSHR / MSAA=RENDER_TARGET / 深度=DEPTH_WRITE）
    DX_CHECK(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                              D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                              IID_PPV_ARGS(&_resource)),
             "create texture resource");
    if (!_resource.Get()) {
        LOGE("[DX12] tex desc fmt={} {}x{} mip={} sampleCount={} quality={} flags=0x{:08X}",
             static_cast<uint32_t>(_format), width, height, rd.MipLevels,
             rd.SampleDesc.Count, rd.SampleDesc.Quality, static_cast<uint32_t>(rd.Flags));
    }
    return _resource.Get() != nullptr;
}

bool DXTexture2D::uploadAndGenMips(const TextureDesc& desc, const TextureDataView2D& data) {
    UINT texel = 4;
    std::vector<uint8_t> expanded;
    if (!ExpandToStorage(desc.format, data.data, _width, _height, data.channels, expanded, texel)) {
        return false;
    }

    // 暂存行距须按 D3D12_TEXTURE_DATA_PITCH_ALIGNMENT 对齐
    const size_t rowBytes = static_cast<size_t>(_width) * texel;
    const size_t rowPitch = AlignUp(rowBytes, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    const size_t total = rowPitch * static_cast<size_t>(_height);

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = total;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_UNKNOWN;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> staging;
    DX_CHECK(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                              D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                              IID_PPV_ARGS(&staging)),
             "create texture staging buffer");
    if (!staging.Get()) return false;

    void* mapped = nullptr;
    D3D12_RANGE readRange{0, 0};
    DX_CHECK(staging->Map(0, &readRange, &mapped), "map texture staging");
    if (!mapped) return false;
    for (int row = 0; row < _height; ++row) {
        std::memcpy(static_cast<uint8_t*>(mapped) + rowPitch * row,
                    expanded.data() + rowBytes * row, rowBytes);
    }
    staging->Unmap(0, nullptr);

    // mipgen 描述符堆在录制前创建、fence 等待后析构：SetDescriptorHeaps 绑定的堆
    // 须存活至命令执行完成，故不能在录制 lambda 内部作为局部量
    ComPtr<ID3D12DescriptorHeap> mipSrvHeap;
    ComPtr<ID3D12DescriptorHeap> mipRtvHeap;
    if (_mipLevels > 1 && !CreateMipgenHeaps(mipSrvHeap, mipRtvHeap)) {
        LOGW("[DX12] mipmap descriptor heaps unavailable; clamping to mip0 ({})", _mipLevels);
        _mipLevels = 1;
    }

    const bool ok = executeOneShot([&](ID3D12GraphicsCommandList* cmd) {
        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = _resource.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = staging.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Footprint.Format = _format;
        src.PlacedFootprint.Footprint.Width = static_cast<UINT>(_width);
        src.PlacedFootprint.Footprint.Height = static_cast<UINT>(_height);
        src.PlacedFootprint.Footprint.Depth = 1;
        src.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(rowPitch);
        src.PlacedFootprint.Offset = 0;
        D3D12_BOX box{0, 0, 0, static_cast<UINT>(_width), static_cast<UINT>(_height), 1};
        cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);

        // 整体 COPY_DEST → PSHR（mipgen 内部再做子资源级往返切换）
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = _resource.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd->ResourceBarrier(1, &barrier);

        if (_mipLevels > 1 &&
            !recordMipgen(cmd, mipSrvHeap.ptr, mipRtvHeap.ptr)) {
            LOGW("[DX12] mipmap generation unavailable; clamping to mip0 ({})", _mipLevels);
            _mipLevels = 1;   // SRV 只暴露 mip0，资源多分配的层不影响正确性
        }
    });
    return ok;
}

// mipgen 用瞬态堆：每 mip 一个单层 SRV（shader-visible，根表绑定）+ 一个 RTV
bool DXTexture2D::CreateMipgenHeaps(ComPtr<ID3D12DescriptorHeap>& srvOut,
                                    ComPtr<ID3D12DescriptorHeap>& rtvOut) {
    D3D12_DESCRIPTOR_HEAP_DESC shd{};
    shd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    shd.NumDescriptors = _mipLevels;
    shd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    DX_CHECK(_device->CreateDescriptorHeap(&shd, IID_PPV_ARGS(&srvOut)), "mipgen srv heap");
    if (!srvOut.Get()) return false;
    D3D12_DESCRIPTOR_HEAP_DESC rhd{};
    rhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rhd.NumDescriptors = _mipLevels;
    DX_CHECK(_device->CreateDescriptorHeap(&rhd, IID_PPV_ARGS(&rtvOut)), "mipgen rtv heap");
    return rtvOut.Get() != nullptr;
}

// 手动 mipmap 生成：mipdown PSO（Gather 角点等权盒平均，对齐 vkCmdBlitImage
// linear 的 2:1 盒式语义）全屏三角形逐级降采样。每级先把该子资源切到
// RENDER_TARGET 写入，画完立即切回 PIXEL_SHADER_RESOURCE 供下一级采样。
// 堆生命周期由调用方保证（存活至命令执行完成）。
bool DXTexture2D::recordMipgen(ID3D12GraphicsCommandList* cmd,
                               ID3D12DescriptorHeap* srvHeap,
                               ID3D12DescriptorHeap* rtvHeap) {
    if (!_blitCtx || !srvHeap || !rtvHeap) return false;
    ID3D12RootSignature* rs = _blitCtx->BlitRootSignature();
    // 盒平均专用 PSO（线性 Sample 的 blit PSO 留给 RT↔RT 颜色拷贝的恒等映射）
    ID3D12PipelineState* pso = _blitCtx->MipdownPsoFor(_srvFormat);
    if (!rs || !pso) return false;

    const UINT srvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const UINT rtvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    const auto srvBase = srvHeap->GetGPUDescriptorHandleForHeapStart();
    const auto rtvBase = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < _mipLevels; ++i) {
        D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = _srvFormat;
        sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sd.Texture2D.MostDetailedMip = i;
        sd.Texture2D.MipLevels = 1;
        D3D12_CPU_DESCRIPTOR_HANDLE scpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
        scpu.ptr += i * srvInc;
        _device->CreateShaderResourceView(_resource.Get(), &sd, scpu);

        D3D12_RENDER_TARGET_VIEW_DESC rd{};
        rd.Format = _srvFormat;
        rd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rd.Texture2D.MipSlice = i;
        D3D12_CPU_DESCRIPTOR_HANDLE rcpu = rtvBase;
        rcpu.ptr += i * rtvInc;
        _device->CreateRenderTargetView(_resource.Get(), &rd, rcpu);
    }

    ID3D12DescriptorHeap* heaps[] = {srvHeap};
    cmd->SetDescriptorHeaps(1, heaps);
    cmd->SetGraphicsRootSignature(rs);
    // SV_VertexID 全屏三角形：拓扑须显式声明（默认 UNDEFINED 是调试层 #719 类噪音源）
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    int mw = _width;
    int mh = _height;
    for (UINT i = 1; i < _mipLevels; ++i) {
        mw = std::max(1, mw / 2);
        mh = std::max(1, mh / 2);

        D3D12_RESOURCE_BARRIER toRT{};
        toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toRT.Transition.pResource = _resource.Get();
        toRT.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        toRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        toRT.Transition.Subresource = i;
        cmd->ResourceBarrier(1, &toRT);

        cmd->SetPipelineState(pso);
        D3D12_GPU_DESCRIPTOR_HANDLE src = srvBase;
        src.ptr += (i - 1) * srvInc;
        cmd->SetGraphicsRootDescriptorTable(0, src);
        cmd->IASetVertexBuffers(0, 0, nullptr);   // SV_VertexID 全屏三角形无顶点流
        const D3D12_VIEWPORT vp{0.0f, 0.0f, static_cast<FLOAT>(mw), static_cast<FLOAT>(mh), 0.0f, 1.0f};
        const D3D12_RECT sc{0, 0, mw, mh};
        cmd->RSSetViewports(1, &vp);
        cmd->RSSetScissorRects(1, &sc);
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvBase;
        rtv.ptr += i * rtvInc;
        cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        // InstanceCount 必须 ≥1：传 0 是合法 no-op，mip 链会整条保持未写入的
        // 未定义内容（隐式 LOD 采样即得黑/噪 mip——SkyBox/Defer 变暗根因）
        cmd->DrawInstanced(3, 1, 0, 0);

        D3D12_RESOURCE_BARRIER toSRV = toRT;
        toSRV.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        toSRV.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmd->ResourceBarrier(1, &toSRV);
    }
    return true;
}

bool DXTexture2D::executeOneShot(const std::function<void(ID3D12GraphicsCommandList*)>& record) {
    if (!_queue || !_uploadAlloc || !_uploadFence) return false;
    DX_CHECK(_uploadAlloc->Reset(), "reset upload allocator");
    ComPtr<ID3D12GraphicsCommandList> cmd;
    DX_CHECK(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _uploadAlloc,
                                        nullptr, IID_PPV_ARGS(&cmd)),
             "create texture command list");
    if (!cmd.Get()) return false;

    record(cmd.ptr);

    DX_CHECK(cmd->Close(), "close texture command list");
    ID3D12CommandList* lists[] = {cmd.Get()};
    _queue->ExecuteCommandLists(1, lists);
    // 共享 fence 单调推进等待（同 DXBuffer::waitForGpu 约定）
    const UINT64 value = _uploadFence->GetCompletedValue() + 1;
    _queue->Signal(_uploadFence, value);
    if (_uploadFence->GetCompletedValue() < value) {
        _uploadFence->SetEventOnCompletion(value, _fenceEvent);
        WaitForSingleObject(_fenceEvent, INFINITE);
    }
    return true;
}

void DXTexture2D::setBorderColor(const float bc[4]) {
    if (!bc) return;
    _borderColor = {bc[0], bc[1], bc[2], bc[3]};
}

void DXTexture2D::bind(unsigned int) {
    // 绑定发生在 Renderer 侧：bindTexture 把 SRV 写进共享堆槽 unit+1，
    // draw 前 SetDescriptorHeaps+根表绑定（Task 6 主循环已具备）
}

void* DXTexture2D::handle() { return _resource.Get(); }

void DXTexture2D::release() {
    _resource = {};
    _format = DXGI_FORMAT_UNKNOWN;
    _srvFormat = DXGI_FORMAT_UNKNOWN;
    _mipLevels = 1;
    _width = 0;
    _height = 0;
    _msaa = false;
    _valid = false;
}

} // namespace rhi
