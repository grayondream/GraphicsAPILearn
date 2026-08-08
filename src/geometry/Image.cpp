#include "Image.hpp"
extern "C" {
	#define STB_IMAGE_IMPLEMENTATION
	#include "stb_image.h"
}

#include <utils/GL/GLUtils.hpp>
#include <exception>

Image::Image(const std::string& file, const TextureOption& option) {
	m_file = file;
	m_option = option;
}

Image::~Image() {
	stbi_image_free(m_pdata);
}

Image& Image::load(bool flip) {
	stbi_set_flip_vertically_on_load(flip);
	if (m_option.IsHdr) {
		float *ptr = stbi_loadf(m_file.c_str(), &m_size.width, &m_size.height, &m_size.channel, 0);
		m_pdata = reinterpret_cast<uint8_t*>(ptr);
	}else {
		m_pdata = stbi_load(m_file.c_str(), &m_size.width, &m_size.height, &m_size.channel, 0);
	}

	m_format = GLUtils::PixelChannel2PixelFormat(m_size.channel, m_option.IsHdr);
	return *this;
}

uint8_t* Image::data() {
	return m_pdata;
}

ImageSize Image::size() {
	return m_size;
}

PixelFormat Image::format() {
	return m_format;
}
