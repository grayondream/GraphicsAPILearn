#include "Shape/Image.hpp"
#include "GLTexture2D.hpp"
#include "glad/glad.h"
#include <cassert>

GLImageTexture2D::GLImageTexture2D(const std::string& file) {
	_img = Image(file);
	_coord = {
		Position2D{1.0, 1.0},
		Position2D{1.0, 0.0},
		Position2D{0.0, 0.0},
		Position2D{0.0, 1.0},
	};
}

unsigned int GLImageTexture2D::generateTextureFrom(const uint8_t* data, const int width, const int height) {
	unsigned int texture{};
	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	// set the texture wrapping parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// set texture filtering parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	//glBindTexture(GL_TEXTURE_2D, 0);
	return texture;
}

GLImageTexture2D& GLImageTexture2D::load() {
	auto pdata = _img.load().data();
	if (!pdata) {
		_texture = GL_INVALID_INDEX;
		return *this;
	}

	_texture = generateTextureFrom(pdata, _img.size().width, _img.size().height);
	return *this;
}

unsigned int GLImageTexture2D::texture() {
	return _texture;
}

float* GLImageTexture2D::coord() {
	return reinterpret_cast<float*>(_coord.data());
}

std::size_t GLImageTexture2D::coordSize() {
	return _coord.size() * Position2D::ByteSize;
}

GLImageTexture2D& GLImageTexture2D::multiSurface(const int cnt) {
	if (cnt == 1) {
		return *this;
	}

	_coord.clear();
	//the index is not correct
	// 正面 Front face
	_coord.push_back(Position2D{ 0.0f, 0.0f }); // 左下角
	_coord.push_back(Position2D{ 1.0f, 0.0f }); // 右下角
	_coord.push_back(Position2D{ 1.0f, 1.0f }); // 右上角
	_coord.push_back(Position2D{ 0.0f, 1.0f }); // 左上角

	// 背面 Back face
	_coord.push_back(Position2D{ 1.0f, 1.0f }); // 左下角
	_coord.push_back(Position2D{ 0.0f, 1.0f }); // 右下角
	_coord.push_back(Position2D{ 0.0f, 0.0f }); // 右上角
	_coord.push_back(Position2D{ 1.0f, 0.0f }); // 左上角

	// 底面 Bottom face
	_coord.push_back(Position2D{ 0.0f, 1.0f }); // 左下角
	_coord.push_back(Position2D{ 1.0f, 1.0f }); // 右下角
	_coord.push_back(Position2D{ 1.0f, 0.0f }); // 右上角
	_coord.push_back(Position2D{ 0.0f, 0.0f }); // 左上角

	// 顶面 Top face
	_coord.push_back(Position2D{ 1.0f, 0.0f }); // 左下角
	_coord.push_back(Position2D{ 1.0f, 1.0f }); // 右下角
	_coord.push_back(Position2D{ 0.0f, 1.0f }); // 右上角
	_coord.push_back(Position2D{ 0.0f, 0.0f }); // 左上角

	// 左面 Left face
	_coord.push_back(Position2D{ 0.0f, 1.0f }); // 左下角
	_coord.push_back(Position2D{ 0.0f, 0.0f }); // 右下角
	_coord.push_back(Position2D{ 1.0f, 0.0f }); // 右上角
	_coord.push_back(Position2D{ 1.0f, 1.0f }); // 左上角

	// 右面 Right face
	_coord.push_back(Position2D{ 1.0f, 0.0f }); // 左下角
	_coord.push_back(Position2D{ 0.0f, 0.0f }); // 右下角
	_coord.push_back(Position2D{ 0.0f, 1.0f }); // 右上角
	_coord.push_back(Position2D{ 1.0f, 1.0f }); // 左上角



	return *this;
}