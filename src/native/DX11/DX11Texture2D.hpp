#pragma once
#include <native/TextureBase.hpp>
#include <native/TextureDataView.hpp>
#include <base/DXH.hpp>

using Microsoft::WRL::ComPtr;
class ID3D11Device;
class ID3D11Texture2D;
class DX11Texture2D : public Texture2DBase {
public:
	using Texture2DType = unsigned int;
public:
	DX11Texture2D() = default;
	DX11Texture2D(const ComPtr<ID3D11DeviceContext> ctx, const ComPtr<ID3D11Device>& pdevice) {
		setContext(ctx, pdevice);
	}

	DX11Texture2D& setContext(const ComPtr<ID3D11DeviceContext> ctx, const ComPtr<ID3D11Device>& pdevice) {
		_pDevice = pdevice;
		_pContext = ctx;
		return *this;
	}

	virtual ~DX11Texture2D();

	virtual bool init(const Texture2DDataView& data) override;

	bool init(const std::string& file);

	virtual void bind(const unsigned int unit) override;

	virtual void release() override;

	virtual void* handle() override;

	virtual bool valid() override;

private:
	ComPtr<ID3D11DeviceContext> _pContext;
	ComPtr<ID3D11Device> _pDevice{};
	ComPtr<ID3D11Texture2D> _ptexture{};
	ComPtr<ID3D11ShaderResourceView> _pshaderView{};
};