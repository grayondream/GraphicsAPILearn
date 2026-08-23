#include "rhi/dx12/DXTexture3D.hpp"
#include "rhi/dx12/DXFormat.hpp"
#include "base/Log.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace rhi {

namespace {

// 标准链长 = floor(log2(max(w,h))) + 1（与 GL/VK 分配一致）
UINT ComputeMipLevels(int w, int h) {
    int m = std::max(w, h);
    UINT levels = 1;
    while (m > 1) { m >>= 1; ++levels; }
    return levels;
}

size_t AlignUp(size_t v, size_t a) { return (v + a - 1) / a * a; }

// CPU 上传仅支持 RGBA8 族（LoadCube 的 4 通道强制加载已保证）；ch==3 兜底补 alpha
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
        LOGE("[DX12] unsupported channel count {} for cube upload", ch);
        return false;
    }
    return true;
}

} // namespace

DXTexture3D::DXTexture3D(ID3D12Device* device, ID3D12CommandQueue* queue,
                         ID3D12CommandAllocator* uploadAlloc, ID3D12Fence* uploadFence,
                         HANDLE fenceEvent)
    : _device(device)
    , _queue(queue)
    , _uploadAlloc(uploadAlloc)
    , _uploadFence(uploadFence)
    , _fenceEvent(fenceEvent) {}

DXTexture3D::~DXTexture3D() { release(); }

bool DXTexture3D::init(const TextureDataView3D& data) {
    release();
    if (!_device || !data.data || data.width <= 0 || data.height <= 0) return false;
    _depthSlices = data.depth > 0 ? data.depth : 1;

    // 旧签名按 RGBA8 承载（同 VK e3D 路径的 4 分量存储布局）；
    // z 切片行主序连续排布，等价于 height*depth 行的 2D 图展开
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

    // 暂存行距按 D3D12 纹理行对齐；z 切片在 footprint 内连续排布
    const size_t rowBytes = static_cast<size_t>(_width) * 4;
    const size_t rowPitch = AlignUp(rowBytes, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    const size_t total = rowPitch * static_cast<size_t>(_height) * static_cast<size_t>(_depthSlices);

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    rd.Width = static_cast<UINT>(_width);
    rd.Height = static_cast<UINT>(_height);
    rd.DepthOrArraySize = static_cast<UINT16>(_depthSlices);
    rd.MipLevels = 1;
    rd.Format = _format;
    rd.SampleDesc.Count = 1;
    DX_CHECK(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                              D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                              IID_PPV_ARGS(&_resource)),
             "create texture3d resource");
    if (!_resource.Get()) return false;

    ComPtr<ID3D12Resource> staging;
    D3D12_HEAP_PROPERTIES shp{};
    shp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC srd{};
    srd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    srd.Width = total;
    srd.Height = 1;
    srd.DepthOrArraySize = 1;
    srd.MipLevels = 1;
    srd.SampleDesc.Count = 1;
    srd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    DX_CHECK(_device->CreateCommittedResource(&shp, D3D12_HEAP_FLAG_NONE, &srd,
                                              D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                              IID_PPV_ARGS(&staging)),
             "create texture3d staging");
    if (!staging.Get()) return false;

    void* mapped = nullptr;
    D3D12_RANGE readRange{0, 0};
    DX_CHECK(staging->Map(0, &readRange, &mapped), "map texture3d staging");
    if (!mapped) return false;
    for (int row = 0; row < _height * _depthSlices; ++row) {
        std::memcpy(static_cast<uint8_t*>(mapped) + rowPitch * row,
                    expanded.data() + rowBytes * row, rowBytes);
    }
    staging->Unmap(0, nullptr);

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
        src.PlacedFootprint.Footprint.Depth = static_cast<UINT>(_depthSlices);
        src.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(rowPitch);
        D3D12_BOX box{0, 0, 0, static_cast<UINT>(_width), static_cast<UINT>(_height),
                      static_cast<UINT>(_depthSlices)};
        cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = _resource.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd->ResourceBarrier(1, &barrier);
    });
    if (!ok) return false;
    _valid = true;
    return true;
}

bool DXTexture3D::initCube(const TextureDesc& desc, const TextureDataView2D* faces) {
    release();
    if (!_device || !faces) return false;
    for (int f = 0; f < 6; ++f) {
        if (!faces[f].data || faces[f].width <= 0 || faces[f].height <= 0) return false;
    }

    // CPU 上传仅支持 RGBA8 存储（RGB8 以 RGBA8 承载，展开兜底同 DXTexture2D）
    if (desc.format != TextureFormat::RGBA8 && desc.format != TextureFormat::RGB8) {
        LOGE("[DX12] initCube CPU upload only supports RGBA8, got {}", static_cast<int>(desc.format));
        return false;
    }
    _params = desc;
    _cube = true;
    _format = DXGI_FORMAT_R8G8B8A8_UNORM;
    _srvFormat = _format;
    _rtvFormat = _format;   // 颜色 cube 可再经 attachCubeFace 渲染（RTV 视图同格式）
    _width = faces[0].width;
    _height = faces[0].height;
    _mipLevels = desc.generateMipmap ? ComputeMipLevels(_width, _height) : 1;

    // mip 链需渲染目标标志供降采样 RTV；单 mip 仅采样无额外标志
    D3D12_RESOURCE_FLAGS flags =
        _mipLevels > 1 ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE;
    if (!createCubeStorage(_width, _height, _mipLevels, flags)) return false;
    if (!uploadFaces(faces)) return false;
    _valid = true;   // 先置位：genCubeMipmaps 以 valid 为门禁
    if (_mipLevels > 1) genCubeMipmaps();   // 独立提交：内部做子资源级往返切换
    return true;
}

bool DXTexture3D::createEmpty(const TextureDesc& desc, int width, int height) {
    release();
    if (!_device || width <= 0 || height <= 0) return false;

    _params = desc;
    _cube = true;
    _depth = desc.format == TextureFormat::Depth32F ||
             desc.format == TextureFormat::Depth24Stencil8;
    _format = DXFormatOf(desc.format);
    // 视图 typed 格式：TYPELESS 资源建视图必须给具体格式（同 DXTexture2D 约定）
    if (desc.format == TextureFormat::Depth32F) {
        _srvFormat = DXGI_FORMAT_R32_FLOAT;
        _dsvFormat = DXGI_FORMAT_D32_FLOAT;
    } else if (desc.format == TextureFormat::Depth24Stencil8) {
        _srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        _dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    } else {
        _srvFormat = _format;
        _rtvFormat = _format;
    }
    _width = width;
    _height = height;
    _mipLevels = desc.generateMipmap ? ComputeMipLevels(width, height) : 1;

    // 深度=深度模板附件标志；颜色恒带渲染目标标志（attachCubeFace 渲入面的
    // RTV 视图要求，如 IBL envCubemap/irradiance/prefilter 捕获）
    D3D12_RESOURCE_FLAGS flags =
        _depth ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if (!createCubeStorage(width, height, _mipLevels, flags)) return false;

    // 资源以 COPY_DEST 创建，立即转常驻态：深度=DEPTH_WRITE 供阴影 pass 写入，
    // 颜色=PSHR 供采样（attachCubeFace 渲染时由 Renderer 做子资源级往返转移）
    const D3D12_RESOURCE_STATES finalState = _depth
        ? D3D12_RESOURCE_STATE_DEPTH_WRITE
        : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
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

bool DXTexture3D::createCubeStorage(int width, int height, UINT mips, D3D12_RESOURCE_FLAGS flags) {
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = static_cast<UINT>(width);
    rd.Height = static_cast<UINT>(height);
    rd.DepthOrArraySize = 6;
    rd.MipLevels = static_cast<UINT16>(mips);
    rd.Format = _format;
    rd.SampleDesc.Count = 1;
    rd.Flags = flags;
    // 统一以 COPY_DEST 创建；调用方负责转常驻态
    DX_CHECK(_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                              D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                              IID_PPV_ARGS(&_resource)),
             "create cube resource");
    return _resource.Get() != nullptr;
}

bool DXTexture3D::uploadFaces(const TextureDataView2D* faces) {
    // 每面独立暂存缓冲，行距按 256 对齐逐行填充。暂存在本函数作用域创建
    // （executeOneShot 同步等待 fence 完成后才返回），不能放录制 lambda 内——
    // lambda 返回即析构、命令执行时 GPU 读已释放资源（Task 7 同类教训）
    const size_t rowBytes = static_cast<size_t>(_width) * 4;
    const size_t rowPitch = AlignUp(rowBytes, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    const size_t faceTotal = rowPitch * static_cast<size_t>(_height);

    std::array<ComPtr<ID3D12Resource>, 6> stagings{};
    std::vector<uint8_t> expanded;
    for (int f = 0; f < 6; ++f) {
        if (!ExpandRgba8(faces[f].data, faces[f].width, faces[f].height,
                         faces[f].channels > 0 ? faces[f].channels : 4, expanded)) {
            continue;   // 已 LOGE；该面不上传（内容为未定义）
        }
        D3D12_HEAP_PROPERTIES shp{};
        shp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC srd{};
        srd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        srd.Width = faceTotal;
        srd.Height = 1;
        srd.DepthOrArraySize = 1;
        srd.MipLevels = 1;
        srd.SampleDesc.Count = 1;
        srd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        DX_CHECK(_device->CreateCommittedResource(&shp, D3D12_HEAP_FLAG_NONE, &srd,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                  IID_PPV_ARGS(&stagings[static_cast<size_t>(f)])),
                 "create cube face staging");
        auto& staging = stagings[static_cast<size_t>(f)];
        if (!staging.Get()) continue;
        void* mapped = nullptr;
        D3D12_RANGE readRange{0, 0};
        if (FAILED(staging->Map(0, &readRange, &mapped)) || !mapped) continue;
        for (int row = 0; row < _height; ++row) {
            std::memcpy(static_cast<uint8_t*>(mapped) + rowPitch * row,
                        expanded.data() + rowBytes * row, rowBytes);
        }
        staging->Unmap(0, nullptr);
    }

    return executeOneShot([&](ID3D12GraphicsCommandList* cmd) {
        for (int f = 0; f < 6; ++f) {
            if (!stagings[static_cast<size_t>(f)].Get()) continue;
            D3D12_TEXTURE_COPY_LOCATION dst{};
            dst.pResource = _resource.Get();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = static_cast<UINT>(f) * _mipLevels;   // 面 mip0
            D3D12_TEXTURE_COPY_LOCATION src{};
            src.pResource = stagings[static_cast<size_t>(f)].Get();
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint.Footprint.Format = _format;
            src.PlacedFootprint.Footprint.Width = static_cast<UINT>(_width);
            src.PlacedFootprint.Footprint.Height = static_cast<UINT>(_height);
            src.PlacedFootprint.Footprint.Depth = 1;
            src.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(rowPitch);
            D3D12_BOX box{0, 0, 0, static_cast<UINT>(_width), static_cast<UINT>(_height), 1};
            cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
        }

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = _resource.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd->ResourceBarrier(1, &barrier);
    });
}

void DXTexture3D::genCubeMipmaps() {
    if (!_valid || !_cube || _depth || _mipLevels <= 1) return;
    if (!_blitCtx) {
        LOGW("[DX12] genCubeMipmaps without blit context; clamping to mip0 ({})", _mipLevels);
        _mipLevels = 1;
        return;
    }

    // 瞬态堆：每 (face,mip) 一个单层 SRV（shader-visible）+ 一个 RTV；
    // 须存活至命令执行完成（同 DXTexture2D mipgen 堆生命周期约定）
    UINT srvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    UINT rtvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const UINT slots = 6u * _mipLevels;

    ComPtr<ID3D12DescriptorHeap> srvHeap;
    D3D12_DESCRIPTOR_HEAP_DESC shd{};
    shd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    shd.NumDescriptors = slots;
    shd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    DX_CHECK(_device->CreateDescriptorHeap(&shd, IID_PPV_ARGS(&srvHeap)), "cubemip srv heap");
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    D3D12_DESCRIPTOR_HEAP_DESC rhd{};
    rhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rhd.NumDescriptors = slots;
    DX_CHECK(_device->CreateDescriptorHeap(&rhd, IID_PPV_ARGS(&rtvHeap)), "cubemip rtv heap");
    if (!srvHeap.Get() || !rtvHeap.Get()) {
        LOGW("[DX12] cube mipmap heaps unavailable; clamping to mip0 ({})", _mipLevels);
        _mipLevels = 1;
        return;
    }

    auto scpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
    auto rcpu = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT m = 0; m < _mipLevels; ++m) {
        for (UINT f = 0; f < 6; ++f) {
            const UINT idx = f * _mipLevels + m;
            D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
            sd.Format = _srvFormat;
            sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;   // 单面单 mip 视为 2D
            sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            sd.Texture2DArray.MostDetailedMip = m;
            sd.Texture2DArray.MipLevels = 1;
            sd.Texture2DArray.FirstArraySlice = f;
            sd.Texture2DArray.ArraySize = 1;
            D3D12_CPU_DESCRIPTOR_HANDLE s = scpu;
            s.ptr += static_cast<SIZE_T>(idx) * srvInc;
            _device->CreateShaderResourceView(_resource.Get(), &sd, s);

            D3D12_RENDER_TARGET_VIEW_DESC rd{};
            rd.Format = _rtvFormat;
            rd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            rd.Texture2DArray.MipSlice = m;
            rd.Texture2DArray.FirstArraySlice = f;
            rd.Texture2DArray.ArraySize = 1;
            D3D12_CPU_DESCRIPTOR_HANDLE r = rcpu;
            r.ptr += static_cast<SIZE_T>(idx) * rtvInc;
            _device->CreateRenderTargetView(_resource.Get(), &rd, r);
        }
    }

    const bool gen = executeOneShot([&](ID3D12GraphicsCommandList* cmd) {
        recordCubeMipgen(cmd, srvHeap.ptr, rtvHeap.ptr);
    });
    if (!gen) {
        LOGW("[DX12] cube mipmap generation failed; keeping uploaded mip chain partial ({})", _mipLevels);
    }
}

// 手动 cubemap mipmap：blit PSO 全屏三角形逐面逐级线性降采样。第 i 级先把全部
// 6 个面的该子资源切到 RENDER_TARGET 写入、画完立即切回 PSHR 供下一级采样。
// 堆生命周期由调用方保证（存活至命令执行完成）。
bool DXTexture3D::recordCubeMipgen(ID3D12GraphicsCommandList* cmd,
                                   ID3D12DescriptorHeap* srvHeap,
                                   ID3D12DescriptorHeap* rtvHeap) {
    if (!_blitCtx || !srvHeap || !rtvHeap) return false;
    ID3D12RootSignature* rs = _blitCtx->BlitRootSignature();
    // 源 SRV 是 TEXTURE2DARRAY 视图（单面单 mip）：必须用数组变体 PSO，
    // Texture2D 声明采样数组视图属未定义行为（读到的切片/内容不可靠）
    ID3D12PipelineState* pso = _blitCtx->BlitArrayPsoFor(_srvFormat);
    if (!rs || !pso) return false;

    const UINT srvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const UINT rtvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto srvBase = srvHeap->GetGPUDescriptorHandleForHeapStart();
    auto rtvBase = rtvHeap->GetCPUDescriptorHandleForHeapStart();

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

        std::vector<D3D12_RESOURCE_BARRIER> toRT;
        toRT.reserve(6);
        for (UINT f = 0; f < 6; ++f) {
            D3D12_RESOURCE_BARRIER b{};
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Transition.pResource = _resource.Get();
            b.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            b.Transition.Subresource = f * _mipLevels + i;
            toRT.push_back(b);
        }
        cmd->ResourceBarrier(static_cast<UINT>(toRT.size()), toRT.data());

        cmd->SetPipelineState(pso);
        for (UINT f = 0; f < 6; ++f) {
            D3D12_GPU_DESCRIPTOR_HANDLE src = srvBase;
            src.ptr += static_cast<SIZE_T>(f * _mipLevels + (i - 1)) * srvInc;
            cmd->SetGraphicsRootDescriptorTable(0, src);
            cmd->IASetVertexBuffers(0, 0, nullptr);   // SV_VertexID 全屏三角形无顶点流
            const D3D12_VIEWPORT vp{0.0f, 0.0f, static_cast<FLOAT>(mw), static_cast<FLOAT>(mh), 0.0f, 1.0f};
            const D3D12_RECT sc{0, 0, mw, mh};
            cmd->RSSetViewports(1, &vp);
            cmd->RSSetScissorRects(1, &sc);
            D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvBase;
            rtv.ptr += static_cast<SIZE_T>(f * _mipLevels + i) * rtvInc;
            cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
            // InstanceCount 必须 ≥1：传 0 是合法 no-op，mip 链会整条保持未写入的
            // 未定义内容（隐式 LOD 采样即得黑/噪 mip——SkyBox 变暗根因）
            cmd->DrawInstanced(3, 1, 0, 0);
        }

        for (auto& b : toRT) std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
        cmd->ResourceBarrier(static_cast<UINT>(toRT.size()), toRT.data());
    }
    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXTexture3D::rtvFace(int face, int mip) {
    D3D12_CPU_DESCRIPTOR_HANDLE nullHandle{};
    if (!_valid || _depth || !_resource.Get() || face < 0 || face > 5 ||
        mip < 0 || static_cast<UINT>(mip) >= _mipLevels) {
        return nullHandle;
    }
    const UINT rtvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    if (!_faceRtvHeap.Get()) {
        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hd.NumDescriptors = 6u * _mipLevels;
        DX_CHECK(_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_faceRtvHeap)),
                 "create cube face rtv heap");
        if (!_faceRtvHeap.Get()) return nullHandle;
        auto base = _faceRtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT m = 0; m < _mipLevels; ++m) {
            for (UINT f = 0; f < 6; ++f) {
                D3D12_RENDER_TARGET_VIEW_DESC rd{};
                rd.Format = _rtvFormat;
                rd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                rd.Texture2DArray.MipSlice = m;
                rd.Texture2DArray.FirstArraySlice = f;
                rd.Texture2DArray.ArraySize = 1;
                D3D12_CPU_DESCRIPTOR_HANDLE h = base;
                h.ptr += static_cast<SIZE_T>(f * _mipLevels + m) * rtvInc;
                _device->CreateRenderTargetView(_resource.Get(), &rd, h);
            }
        }
    }
    D3D12_CPU_DESCRIPTOR_HANDLE h = _faceRtvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<SIZE_T>(subresource(face, mip)) * rtvInc;
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXTexture3D::dsvFace(int face) {
    D3D12_CPU_DESCRIPTOR_HANDLE nullHandle{};
    if (!_valid || !_depth || !_resource.Get() || face < 0 || face > 5) {
        return nullHandle;
    }
    const UINT dsvInc = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    if (!_faceDsvHeap.Get()) {
        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        hd.NumDescriptors = 6;
        DX_CHECK(_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&_faceDsvHeap)),
                 "create cube face dsv heap");
        if (!_faceDsvHeap.Get()) return nullHandle;
        auto base = _faceDsvHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT f = 0; f < 6; ++f) {
            D3D12_DEPTH_STENCIL_VIEW_DESC dd{};
            dd.Format = _dsvFormat;
            dd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dd.Texture2DArray.MipSlice = 0;
            dd.Texture2DArray.FirstArraySlice = f;
            dd.Texture2DArray.ArraySize = 1;
            D3D12_CPU_DESCRIPTOR_HANDLE h = base;
            h.ptr += static_cast<SIZE_T>(f) * dsvInc;
            _device->CreateDepthStencilView(_resource.Get(), &dd, h);
        }
    }
    D3D12_CPU_DESCRIPTOR_HANDLE h = _faceDsvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<SIZE_T>(face) * dsvInc;
    return h;
}

bool DXTexture3D::WriteSrv(D3D12_CPU_DESCRIPTOR_HANDLE dst) {
    if (!_valid || !_resource.Get()) return false;
    D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = _srvFormat;
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if (_cube) {
        sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        sd.TextureCube.MostDetailedMip = 0;
        sd.TextureCube.MipLevels = _mipLevels;
    } else {
        sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        sd.Texture3D.MostDetailedMip = 0;
        sd.Texture3D.MipLevels = 1;
    }
    _device->CreateShaderResourceView(_resource.Get(), &sd, dst);
    return true;
}

void DXTexture3D::bind(unsigned int) {
    // 绑定发生在 Renderer 侧：bindTexture 把 cubemap SRV 写进共享堆槽 unit+1
}

void* DXTexture3D::handle() { return _resource.Get(); }

bool DXTexture3D::executeOneShot(const std::function<void(ID3D12GraphicsCommandList*)>& record) {
    if (!_queue || !_uploadAlloc || !_uploadFence) return false;
    DX_CHECK(_uploadAlloc->Reset(), "reset upload allocator");
    ComPtr<ID3D12GraphicsCommandList> cmd;
    DX_CHECK(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _uploadAlloc,
                                        nullptr, IID_PPV_ARGS(&cmd)),
             "create texture3d command list");
    if (!cmd.Get()) return false;

    record(cmd.ptr);

    DX_CHECK(cmd->Close(), "close texture3d command list");
    ID3D12CommandList* lists[] = {cmd.Get()};
    _queue->ExecuteCommandLists(1, lists);
    // 共享 fence 单调推进等待（同 DXBuffer/DXTexture2D 约定）
    const UINT64 value = _uploadFence->GetCompletedValue() + 1;
    _queue->Signal(_uploadFence, value);
    if (_uploadFence->GetCompletedValue() < value) {
        _uploadFence->SetEventOnCompletion(value, _fenceEvent);
        WaitForSingleObject(_fenceEvent, INFINITE);
    }
    return true;
}

void DXTexture3D::release() {
    _faceDsvHeap = {};
    _faceRtvHeap = {};
    _resource = {};
    _format = DXGI_FORMAT_UNKNOWN;
    _srvFormat = DXGI_FORMAT_UNKNOWN;
    _rtvFormat = DXGI_FORMAT_UNKNOWN;
    _dsvFormat = DXGI_FORMAT_UNKNOWN;
    _mipLevels = 1;
    _width = 0;
    _height = 0;
    _depthSlices = 0;
    _cube = false;
    _depth = false;
    _valid = false;
}

} // namespace rhi
