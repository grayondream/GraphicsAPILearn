#include "Image.hpp"
extern "C" {
	#define STB_IMAGE_IMPLEMENTATION
	#include "stb_image.h"
}


Image::Image(const std::string& file) {
	_file = file;
}

Image::~Image() {
	stbi_image_free(_pdata);
}

Image& Image::load() {
	_pdata = stbi_load(_file.c_str(), &_size.width, &_size.height, &_size.channel, 0);
	return *this;
}

uint8_t* Image::data() {
	return _pdata;
}

Size2D Image::size() {
	return _size;
}

