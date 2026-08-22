#pragma once
#include "rhi/dx12/DXHeader.hpp"
#include "rhi/core/Common.hpp"

namespace rhi {

// RGB8 在 DXGI 无 24 位格式，以 R8G8B8A8_UNORM 承载；CPU 侧展开在加载期完成
// （RhiImage::Load2D 统一强制 stbi 4 通道加载，desc.format 已是 RGBA8，同 VK 做法）。
// 深度格式按 TYPELESS 资源创建约定映射，DSV/SRV 视图阶段再取 typed 格式
// （D32_FLOAT/R32_FLOAT），供 Task 5 纹理与 Task 6 渲染目标使用。
constexpr DXGI_FORMAT DXFormatOf(TextureFormat f) {
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

inline uint32_t ToDXTexelSize(TextureFormat format) {
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

} // namespace rhi
