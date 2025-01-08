#pragma once
#include <Native/ImageTexture2D.hpp>
#include <vector>
#include <Base/DXH.hpp>

using Microsoft::WRL::ComPtr;
class ID3D11DeviceContext;
class ID3D11Device;
class DX11ImageTexture2D : public ImageTexture2D{
public:
	DX11ImageTexture2D(const std::string& file);

	DX11ImageTexture2D& setContext(const ComPtr<ID3D11DeviceContext> ctx, const ComPtr<ID3D11Device>& pdevice);

	virtual DX11ImageTexture2D& load() override ;
};