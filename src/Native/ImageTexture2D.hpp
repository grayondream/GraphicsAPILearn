#pragma once
#include <Geometry/Image.hpp>
#include "ITexture2D.hpp"
#include "Geometry/Vertex.hpp"
#include <vector>
#include <memory>

using GLTextureType = unsigned int;
using DX11TextureType = 
class ImageTexture2D {
public:
	ImageTexture2D(const std::string& file, const TextureOption& option = {});

	virtual ImageTexture2D& load();

	float* coord();

	std::size_t coordSize();

	std::shared_ptr<ITexture2D>& texture() {
		return _texture;
	}
private:
	Image _img{};
	std::vector<Point2D> _coord{};

protected:
	std::shared_ptr<ITexture2D> _texture{};
};