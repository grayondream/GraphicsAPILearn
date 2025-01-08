#include "DX11ImageTexture2D.hpp"
#include "Geometry/Image.hpp"
#include <Native/DX11/DX11Texture2D.hpp>
#include "glad/glad.h"
#include <cassert>

DX11ImageTexture2D::DX11ImageTexture2D(const std::string& file) 
	: ImageTexture2D(file) {
	_texture = std::make_shared<DX11Texture2D>();
}


DX11ImageTexture2D& DX11ImageTexture2D::load() {
	ImageTexture2D::load();
	return *this;
}

DX11ImageTexture2D& DX11ImageTexture2D::setContext(const ComPtr<ID3D11DeviceContext> ctx, const ComPtr<ID3D11Device>& pdevice) {
	if (_texture) {
		auto tex = static_cast<DX11Texture2D*>(_texture.get());
		tex->setContext(ctx, pdevice);
	}

	return *this;
}

