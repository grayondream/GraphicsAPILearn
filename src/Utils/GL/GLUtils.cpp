#include "GLUtils.hpp"

namespace GLUtils {
    GLenum PixelChannel2Layout(int channel) {
        switch (channel) {
        case 1:
            return GL_RED;
        case 3:
            return GL_RGB;
        case 4:
            return GL_RGBA;
        default:
            return GL_UNIFORM_TYPE;
        }

        return GL_UNIFORM_TYPE;
    }

    GLenum PixelFormat2GLFormat(const PixelFormat& fmt) {
        switch (fmt) {
        case PixelFormat::RED:
            return GL_RED;break;
        case PixelFormat::RGB:
            return GL_RGB;break;
        case PixelFormat::RGBA:
            return GL_RGBA;break;
        case PixelFormat::RGB16F:
            return GL_RGB16F;break;
        case PixelFormat::RGBA16F:
            return GL_RGBA16F;break;
        case PixelFormat::RGB32F:
            return GL_RGB32F;break;
        case PixelFormat::RGBA32F:
            return GL_RGBA32F;break;
        default:
            return GL_UNSIGNED_BYTE;break;
        } 

        return GL_UNSIGNED_BYTE;
    }

    PixelFormat GLFormat2PixelFormat(const GLenum& fmt) {
        switch (fmt) {
        case GL_RED:
            return PixelFormat::RED;break;
        case GL_RGB:
            return PixelFormat::RGB;break;
        case GL_RGBA:
            return PixelFormat::RGBA;break;
        case GL_RGB16F:
            return PixelFormat::RGB16F;break;
        case GL_RGBA16F:
            return PixelFormat::RGBA16F;break;
        case GL_RGB32F:
            return PixelFormat::RGB32F;break;
        case GL_RGBA32F:
            return PixelFormat::RGBA32F;break;
        default:
            return PixelFormat::Unknown;break; 
        } 

        return PixelFormat::Unknown;
    }

    PixelFormat PixelChannel2PixelFormat(const int channel, bool isHdr) {
        switch (channel) {
        case 1:
            return isHdr ? PixelFormat::RED16F : PixelFormat::RED;break;
        case 3:
            return isHdr ? PixelFormat::RGB16F : PixelFormat::RGB;break;
        case 4:
            return isHdr ? PixelFormat::RGBA16F : PixelFormat::RGBA;break;
        case 16:
            return isHdr ? PixelFormat::RGB32F : PixelFormat::RGB;break;
        case 32:
            return isHdr ? PixelFormat::RGBA32F : PixelFormat::RGBA;break;

        default:
            return PixelFormat::Unknown;break; 
        } 

        return PixelFormat::Unknown;
    }

    GLuint Ptr2GLTextureId(const void* ptr) {
        return *static_cast<const GLuint*>(ptr);
    }

    void* GLTextureId2Ptr(const GLuint& id) {
        return static_cast<void*>(new GLuint(id)); 
    }
}