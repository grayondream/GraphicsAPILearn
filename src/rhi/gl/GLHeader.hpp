#pragma once
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include "rhi/core/Common.hpp"

namespace rhi {

inline GLenum ToGLInternalFormat(TextureFormat f) {
    switch (f) {
        case TextureFormat::RGB8:            return GL_RGB;
        case TextureFormat::RGBA8:           return GL_RGBA;
        case TextureFormat::RGBA16F:         return GL_RGBA16F;
        case TextureFormat::RGB16F:          return GL_RGB16F;
        case TextureFormat::RG16F:           return GL_RG16F;
        case TextureFormat::R32F:            return GL_R32F;
        case TextureFormat::RGBA32F:         return GL_RGBA32F;
        case TextureFormat::Depth32F:        return GL_DEPTH_COMPONENT32F;
        case TextureFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
    }
    return GL_RGBA;
}

inline bool IsDepthFormat(TextureFormat f) {
    return f == TextureFormat::Depth32F || f == TextureFormat::Depth24Stencil8;
}

inline bool IsFloatFormat(TextureFormat f) {
    return f == TextureFormat::RGBA16F || f == TextureFormat::RGB16F ||
           f == TextureFormat::RG16F || f == TextureFormat::R32F ||
           f == TextureFormat::RGBA32F;
}

inline GLenum ToGLFormat(TextureFormat f, int channels) {
    if (f == TextureFormat::Depth24Stencil8) return GL_DEPTH_STENCIL;
    if (f == TextureFormat::Depth32F) return GL_DEPTH_COMPONENT;
    return (channels == 4) ? GL_RGBA : GL_RGB;
}

inline GLenum ToGLSrcType(TextureFormat f) {
    if (IsFloatFormat(f) || f == TextureFormat::Depth32F) return GL_FLOAT;
    if (f == TextureFormat::Depth24Stencil8) return GL_UNSIGNED_INT_24_8;
    return GL_UNSIGNED_BYTE;
}

inline GLenum ToGLWrap(TextureWrap w) {
    switch (w) {
        case TextureWrap::Repeat:        return GL_REPEAT;
        case TextureWrap::ClampToEdge:   return GL_CLAMP_TO_EDGE;
        case TextureWrap::ClampToBorder: return GL_CLAMP_TO_BORDER;
    }
    return GL_REPEAT;
}

inline GLenum ToGLMinFilter(TextureFilter f) {
    switch (f) {
        case TextureFilter::Linear:          return GL_LINEAR;
        case TextureFilter::Nearest:         return GL_NEAREST;
        case TextureFilter::LinearMipLinear: return GL_LINEAR_MIPMAP_LINEAR;
    }
    return GL_LINEAR;
}

} // namespace rhi
