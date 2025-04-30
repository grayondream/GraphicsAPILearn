#include "GLImageTexture3D.hpp"
#include "Geometry/Image.hpp"
#include <Native/GL/GLTexture3D.hpp>
#include "glad/glad.h"
#include <cassert>

GLImageTexture3D::GLImageTexture3D(const std::string& path) 
	: ImageTexture3D(path) {
}


GLImageTexture3D& GLImageTexture3D::load() {
	_texture = std::make_shared<GLTexture3D>();
	ImageTexture3D::load();
	return *this;
}

