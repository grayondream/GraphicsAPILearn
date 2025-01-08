#pragma once
#include <Native/TextureDataView.hpp>
#include <Math/Vector.hpp>
#include <Geometry/Image.hpp>

class ITexture2D {
public:
	virtual ~ITexture2D() = default;

	virtual bool init(const Texture2DDataView &data) = 0;

	virtual void bind(const unsigned int unit = 0) = 0;

	virtual ImageSize size() const = 0;

	virtual void release() = 0;

	virtual void* handle() = 0;
};