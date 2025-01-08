#include "GLImageTexture2D.hpp"
#include "Geometry/Image.hpp"
#include <Native/GL/GLTexture2D.hpp>
#include "glad/glad.h"
#include <cassert>

GLImageTexture2D::GLImageTexture2D(const std::string& file) 
	: ImageTexture2D(file) {
}


GLImageTexture2D& GLImageTexture2D::load() {
	_texture = std::make_shared<GLTexture2D>();
	ImageTexture2D::load();
	return *this;
}

