#include "GLTexture3D.hpp"
#include "glad/glad.h"
#include <cassert>
#include <utils/GL/GLUtils.hpp>

GLTexture3D::~GLTexture3D(){
    
}

bool GLTexture3D::init(const Texture3DDataView &data){
    assert(data.size() == 6 && data[0].length() > 0);
    _size = data[0].size();
    unsigned int textureId{};
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureId);
	int width{}, height{}, channel{};
    auto format = GLUtils::PixelFormat2GLFormat(data[0].format());
	for (auto i = 0; i < data.size(); i++) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, _size.width, _size.height, 0, format, GL_UNSIGNED_BYTE, data[i].data());
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    _textureId = textureId;
    return true;
}

void GLTexture3D::bind(const unsigned int unit){
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, _textureId);
}

void GLTexture3D::release(){
    if (!_textureId) {
        return;
    }

    glDeleteTextures(1, &_textureId);
    _textureId = 0;
}

void* GLTexture3D::handle() {
    return GLUtils::GLTextureId2Ptr(_textureId);
}

bool GLTexture3D::valid() {
    return _textureId != 0;
}
