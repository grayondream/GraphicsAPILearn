#include "GLUtils.hpp"

namespace GLUtils {
    GLenum PixelFormat2GLFormat(const PixelFormat& fmt) {
        switch (fmt) {
        case PixelFormat::RED:
            return GL_RED;break;
        case PixelFormat::RGB:
            return GL_RGB;break;
        case PixelFormat::RGBA:
            return GL_RGBA;break;
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
        default:
            return PixelFormat::Unknown;break; 
        } 

        return PixelFormat::Unknown;
    }

    PixelFormat PixelChannel2PixelFormat(const int channel) {
        switch (channel) {
        case 1:
            return PixelFormat::RED;break;
        case 3:
            return PixelFormat::RGB;break;
        case 4:
            return PixelFormat::RGBA;break;
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