
#include "ImageTexture2D.hpp"
#include "Native/GL/GLTexture2D.hpp"
#include "glad/glad.h"
#include <cassert>

ImageTexture2D::ImageTexture2D(const std::string& file) {
	_img = Image(file);
	_coord = {
		Point2D{1.0, 1.0},
		Point2D{1.0, 0.0},
		Point2D{0.0, 0.0},
		Point2D{0.0, 1.0},
	};
}

ImageTexture2D& ImageTexture2D::load() {
	_img.load();
	const Texture2DDataView data = { _img.data(), _img.size().size(), _img.format(), _img.size()};
	_texture->init(data);
	return *this;
}

float* ImageTexture2D::coord() {
	return reinterpret_cast<float*>(_coord.data());
}

std::size_t ImageTexture2D::coordSize() {
	return _coord.size() * sizeof(Point2D);
}
