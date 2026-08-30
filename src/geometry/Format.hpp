#pragma once

enum class PixelFormat : int {
    Unknown,
    RED,
    RGB,
    RGBA,
    RED16F,
    RGB16F,
    RGBA16F,
    RGB32F,
    RGBA32F,
};

// 通道数 → PixelFormat（后端无关工具，供 geometry/native 共用）
inline PixelFormat PixelChannel2PixelFormat(const int channel, bool isHdr = false) {
    switch (channel) {
        case 1:
            return isHdr ? PixelFormat::RED16F : PixelFormat::RED;
        case 3:
            return isHdr ? PixelFormat::RGB16F : PixelFormat::RGB;
        case 4:
            return isHdr ? PixelFormat::RGBA16F : PixelFormat::RGBA;
        case 16:
            return isHdr ? PixelFormat::RGB32F : PixelFormat::RGB;
        case 32:
            return isHdr ? PixelFormat::RGBA32F : PixelFormat::RGBA;
        default:
            return PixelFormat::Unknown;
    }
}