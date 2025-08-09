#pragma once
#include <string>
#include "Base/Vector.hpp"
#include <Geometry/Format.hpp>

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

class Image {
public:
	Image(const std::string& file = {});
	~Image();

	Image& load(bool flip = true);

	uint8_t* data();

	ImageSize size();

	PixelFormat format();

private:
	std::string _file{};
	uint8_t* _pdata{};
	ImageSize _size{};
	PixelFormat _format{};
};