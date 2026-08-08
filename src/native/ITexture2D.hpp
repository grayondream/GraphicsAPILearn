#pragma once
#include <native/TextureDataView.hpp>
#include "base/Vector.hpp"
#include <geometry/Image.hpp>

class ITexture2D {
public:
	virtual ~ITexture2D() = default;

	virtual bool init(const Texture2DDataView &data) = 0;

	virtual void bind(const unsigned int unit = 0) = 0;

	virtual ImageSize size() const = 0;

	virtual void release() = 0;

	virtual void* handle() = 0;

	virtual bool valid() = 0;
};