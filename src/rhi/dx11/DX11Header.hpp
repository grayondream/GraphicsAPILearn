#pragma once
// WIN32_LEAN_AND_MEAN/NOMINMAX 必须在 windows.h 前：min/max 等宏会破坏同 TU 后续
// 头文件的函数调用，且 dx11 头不需要那些遗留组件。
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>       // IDXGIFactory2/IDXGISwapChain1/CreateSwapchainForHwnd；
                           // IDXGISwapChain3(GetCurrentBackBufferIndex) 需 1_4
#include <d3dcompiler.h>   // D3DCreateBlob（.fxc 产物装载）
#include "base/Log.hpp"
#include "rhi/core/Common.hpp"
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#define DX11_CHECK(hr, msg) \
    do { HRESULT _hr = (hr); if (FAILED(_hr)) { \
        LOGE("[DX11] {} failed hr=0x{:08X}", msg, static_cast<uint32_t>(_hr)); \
        dx11diag::DumpMessages(msg); } } while (0)

namespace rhi {
namespace dx11diag {
// 设备侧诊断：DXRenderer::init 注入 InfoQueue（可选调试层，见 DX11Backend.cpp 的
// GRAPHICSLEARN_DX11_DEBUGLAYER），DX11_CHECK 失败时把设备校验消息带出到应用日志
void SetInfoQueue(ID3D11Device* device);
void DumpMessages(const char* context);
} // namespace dx11diag

template <typename T>
struct Dx11ComPtr {
    T* ptr{nullptr};
    Dx11ComPtr() = default;
    ~Dx11ComPtr() { if (ptr) ptr->Release(); }
    // 禁拷贝防双重 Release；移动后源指针置空
    Dx11ComPtr(const Dx11ComPtr&) = delete;
    Dx11ComPtr& operator=(const Dx11ComPtr&) = delete;
    Dx11ComPtr(Dx11ComPtr&& other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
    Dx11ComPtr& operator=(Dx11ComPtr&& other) noexcept {
        if (this != std::addressof(other)) {
            if (ptr) ptr->Release();
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
    T** operator&() { return &ptr; }
    T* operator->() const { return ptr; }
    T* Get() const { return ptr; }
    // 显式释放并置空（析构语义等价，供 shutdown 路径复用）
    void Reset(T* newPtr = nullptr) {
        if (ptr && ptr != newPtr) ptr->Release();
        ptr = newPtr;
    }
};
using namespace std::string_literals;

// RGB8 在 DXGI 无 24 位格式，以 R8G8B8A8_UNORM 承载（CPU 侧展开在加载期完成，
// 同 VK/DX12 做法）；深度格式按 TYPELESS 资源约定映射，视图阶段再取 typed 格式。
// 数值与 DX12 版 DXFormatOf 一致——两代 API 共用同一组 DXGI_FORMAT 枚举。
constexpr DXGI_FORMAT Dx11FormatOf(TextureFormat f) {
    switch (f) {
        case TextureFormat::RGB8:            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGBA8:           return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGBA16F:         return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case TextureFormat::RGB16F:          return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case TextureFormat::RG16F:           return DXGI_FORMAT_R16G16_FLOAT;
        case TextureFormat::R32F:            return DXGI_FORMAT_R32_FLOAT;
        case TextureFormat::RGBA32F:         return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case TextureFormat::Depth32F:        return DXGI_FORMAT_R32_TYPELESS;
        case TextureFormat::Depth24Stencil8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
    }
    return DXGI_FORMAT_R8G8B8A8_UNORM;
}

inline uint32_t ToDx11TexelSize(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGB8:            return 3;
        case TextureFormat::RGBA8:           return 4;
        case TextureFormat::RGBA16F:         return 8;
        case TextureFormat::RGB16F:          return 8;
        case TextureFormat::RG16F:           return 4;
        case TextureFormat::R32F:            return 4;
        case TextureFormat::RGBA32F:         return 16;
        case TextureFormat::Depth32F:        return 4;
        case TextureFormat::Depth24Stencil8: return 4;
    }
    return 4;
}

// draw 时下发 IASetPrimitiveTopology（对照 DX12 的 ToDxTopology）
inline D3D_PRIMITIVE_TOPOLOGY ToDx11Topology(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::TriangleList:  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveType::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case PrimitiveType::Lines:         return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveType::Points:        return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    }
    return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

// 渲染状态映射（Task 3 状态对象化时消费；本任务先提供纯映射函数）
inline D3D11_COMPARISON_FUNC Dx11Compare(CompareFunc func) {
    switch (func) {
        case CompareFunc::Never:        return D3D11_COMPARISON_NEVER;
        case CompareFunc::Less:         return D3D11_COMPARISON_LESS;
        case CompareFunc::Equal:        return D3D11_COMPARISON_EQUAL;
        case CompareFunc::LessEqual:    return D3D11_COMPARISON_LESS_EQUAL;
        case CompareFunc::Greater:      return D3D11_COMPARISON_GREATER;
        case CompareFunc::NotEqual:     return D3D11_COMPARISON_NOT_EQUAL;
        case CompareFunc::GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
        case CompareFunc::Always:       return D3D11_COMPARISON_ALWAYS;
    }
    return D3D11_COMPARISON_ALWAYS;
}

inline D3D11_STENCIL_OP Dx11StencilOp(StencilOp op) {
    switch (op) {
        case StencilOp::Keep:     return D3D11_STENCIL_OP_KEEP;
        case StencilOp::Zero:     return D3D11_STENCIL_OP_ZERO;
        case StencilOp::Replace:  return D3D11_STENCIL_OP_REPLACE;
        case StencilOp::Incr:     return D3D11_STENCIL_OP_INCR_SAT;
        case StencilOp::Decr:     return D3D11_STENCIL_OP_DECR_SAT;
        case StencilOp::IncrWrap: return D3D11_STENCIL_OP_INCR;
        case StencilOp::DecrWrap: return D3D11_STENCIL_OP_DECR;
    }
    return D3D11_STENCIL_OP_KEEP;
}

// filter/wrap → D3D11 采样器描述映射（对照 DX12 的 DxFilterOf/DxAddressOf；
// 寄存器编号约定 f*3+w 见 res/DX11/_samplers.hlsli）
inline D3D11_FILTER Dx11FilterOf(TextureFilter filter) {
    switch (filter) {
        case TextureFilter::Linear:          return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        case TextureFilter::Nearest:         return D3D11_FILTER_MIN_MAG_MIP_POINT;
        case TextureFilter::LinearMipLinear: return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    }
    return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
}

inline D3D11_TEXTURE_ADDRESS_MODE Dx11AddressOf(TextureWrap wrap) {
    switch (wrap) {
        case TextureWrap::Repeat:        return D3D11_TEXTURE_ADDRESS_WRAP;
        case TextureWrap::ClampToEdge:   return D3D11_TEXTURE_ADDRESS_CLAMP;
        case TextureWrap::ClampToBorder: return D3D11_TEXTURE_ADDRESS_BORDER;
    }
    return D3D11_TEXTURE_ADDRESS_WRAP;
}

inline D3D11_BLEND Dx11Blend(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero:             return D3D11_BLEND_ZERO;
        case BlendFactor::One:              return D3D11_BLEND_ONE;
        case BlendFactor::SrcAlpha:         return D3D11_BLEND_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
        case BlendFactor::SrcColor:         return D3D11_BLEND_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return D3D11_BLEND_INV_SRC_COLOR;
    }
    return D3D11_BLEND_ONE;
}

} // namespace rhi
