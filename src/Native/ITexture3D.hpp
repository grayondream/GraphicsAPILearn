#pragma once
#include <Native/TextureDataView.hpp>
#include <Math/Vector.hpp>
#include <Geometry/Image.hpp>

class ITexture3D {
public:
	virtual ~ITexture3D() = default;

	virtual bool init(const Texture3DDataView &data) = 0;

	virtual void bind(const unsigned int unit = 0) = 0;

	virtual ImageSize size() const = 0;

	virtual void release() = 0;

	virtual void* handle() = 0;

	virtual bool valid() = 0;
};