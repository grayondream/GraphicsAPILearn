#pragma once
#include "VKHeader.hpp"
#include "rhi/core/Common.hpp"

namespace rhi {

inline vk::Format ToVkTextureFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGB8:            return vk::Format::eR8G8B8Unorm;
        case TextureFormat::RGBA8:           return vk::Format::eR8G8B8A8Unorm;
        case TextureFormat::RGBA16F:         return vk::Format::eR16G16B16A16Sfloat;
        case TextureFormat::RGB16F:          return vk::Format::eR16G16B16A16Sfloat;
        case TextureFormat::RG16F:           return vk::Format::eR16G16Sfloat;
        case TextureFormat::R32F:            return vk::Format::eR32Sfloat;
        case TextureFormat::RGBA32F:         return vk::Format::eR32G32B32A32Sfloat;
        case TextureFormat::Depth32F:        return vk::Format::eD32Sfloat;
        case TextureFormat::Depth24Stencil8: return vk::Format::eD24UnormS8Uint;
    }
    return vk::Format::eR8G8B8A8Unorm;
}

inline uint32_t ToVkTexelSize(TextureFormat format) {
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

inline vk::ImageAspectFlags ToVkAspect(TextureFormat format) {
    switch (format) {
        case TextureFormat::Depth32F:        return vk::ImageAspectFlagBits::eDepth;
        case TextureFormat::Depth24Stencil8: return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        default:                             return vk::ImageAspectFlagBits::eColor;
    }
}

inline vk::Filter ToVkFilter(TextureFilter filter) {
    switch (filter) {
        case TextureFilter::Nearest:       return vk::Filter::eNearest;
        case TextureFilter::LinearMipLinear:
        case TextureFilter::Linear:        return vk::Filter::eLinear;
    }
    return vk::Filter::eLinear;
}

inline vk::SamplerAddressMode ToVkWrap(TextureWrap wrap) {
    switch (wrap) {
        case TextureWrap::Repeat:       return vk::SamplerAddressMode::eRepeat;
        case TextureWrap::ClampToEdge:  return vk::SamplerAddressMode::eClampToEdge;
        case TextureWrap::ClampToBorder:return vk::SamplerAddressMode::eClampToBorder;
    }
    return vk::SamplerAddressMode::eRepeat;
}

inline vk::SamplerMipmapMode ToVkMipFilter(TextureFilter filter) {
    return filter == TextureFilter::LinearMipLinear ? vk::SamplerMipmapMode::eLinear
                                                    : vk::SamplerMipmapMode::eNearest;
}

inline uint32_t ComputeMipLevels(int width, int height) {
    uint32_t levels = 1;
    uint32_t w = static_cast<uint32_t>(width);
    uint32_t h = static_cast<uint32_t>(height);
    while (w > 1 || h > 1) { w >>= 1; h >>= 1; levels++; }
    return levels;
}

inline vk::SampleCountFlagBits ToVkSamples(int samples) {
    if (samples >= 8) return vk::SampleCountFlagBits::e8;
    if (samples >= 4) return vk::SampleCountFlagBits::e4;
    if (samples >= 2) return vk::SampleCountFlagBits::e2;
    return vk::SampleCountFlagBits::e1;
}

} // namespace rhi
