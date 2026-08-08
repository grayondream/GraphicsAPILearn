#pragma once
#include <string>
#include "base/Vector.hpp"
#include <geometry/Format.hpp>

class ImageSize : public Vector3DBase<int> {
public:
	ImageSize() {

	}
	ImageSize(const ValueType x, const ValueType y, const ValueType z)
		: Vector3DBase(x, y, z) {}

	int size() {
		return width * height * channel;
	}
};

struct TextureOption {
	bool IsHdr = false;
};

class Image {	
public:
	Image(const std::string& file = {}, const TextureOption& option = {});
	~Image();

	Image& load(bool flip = true);

	uint8_t* data();

	ImageSize size();

	PixelFormat format();

private:
	std::string m_file{};
	uint8_t* m_pdata{};
	ImageSize m_size{};
	PixelFormat m_format{};
	TextureOption m_option{};
};