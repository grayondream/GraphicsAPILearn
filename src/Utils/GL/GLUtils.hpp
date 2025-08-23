#pragma once

#include <glad/glad.h>
#include <Geometry/Format.hpp>

namespace GLUtils {
    GLenum PixelChannel2Layout(int channel);

    GLenum PixelFormat2GLFormat(const PixelFormat& fmt);

    PixelFormat GLFormat2PixelFormat(const GLenum& fmt); 

    PixelFormat PixelChannel2PixelFormat(const int channel, bool isHdr = false);

    GLuint Ptr2GLTextureId(const void* ptr);

    void* GLTextureId2Ptr(const GLuint& id);
}
