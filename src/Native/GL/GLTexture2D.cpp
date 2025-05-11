#include "GLTexture2D.hpp"
#include "glad/glad.h"
#include <cassert>
#include <Utils/GL/GLUtils.hpp>

//TODO:
GLTexture2D::~GLTexture2D(){
}

bool GLTexture2D::init(const Texture2DDataView &data){
    assert(data.data() && data.size().length() != 0);
    _size = data.size();
    unsigned int texture{};
    auto format = GLUtils::PixelFormat2GLFormat(data.format());
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	// set the texture wrapping parameters
    auto type = format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, type);	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, type);
	// set texture filtering parameters
    
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, format, size().width, size().height, 0, format, GL_UNSIGNED_BYTE, data.data());
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
    _textureId = texture;
    return true;
}

void GLTexture2D::bind(const unsigned int unit){
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, _textureId);
}

void GLTexture2D::release(){
    if (!_textureId) {
        return;
    }

    glDeleteTextures(1, &_textureId);
    _textureId = 0;
}

void* GLTexture2D::handle() {
    return GLUtils::GLTextureId2Ptr(_textureId);
    //return reinterpret_cast<void*>(static_cast<uintptr_t>(_textureId));
}

bool GLTexture2D::valid() {
    return _textureId != 0;
}
