#include "Image.hpp"
extern "C" {
	#define STB_IMAGE_IMPLEMENTATION
	#include "stb_image.h"
}

#include <Utils/GL/GLUtils.hpp>

Image::Image(const std::string& file) {
	_file = file;
}

Image::~Image() {
	stbi_image_free(_pdata);
}

Image& Image::load(bool flip) {
	stbi_set_flip_vertically_on_load(flip);
	_pdata = stbi_load(_file.c_str(), &_size.width, &_size.height, &_size.channel, 0);
	_format = GLUtils::PixelChannel2PixelFormat(_size.channel);
	return *this;
}

uint8_t* Image::data() {
	return _pdata;
}

ImageSize Image::size() {
	return _size;
}

PixelFormat Image::format() {
	return _format;
}
