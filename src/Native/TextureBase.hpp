#pragma once

#include "Native/ITexture2D.hpp"
#include <cassert>

class Texture2DBase : public ITexture2D{
public:
	virtual ~Texture2DBase() = default;

	virtual ImageSize size() const override {
		return _size;
	}

public:
	ImageSize _size{};
};