#pragma once
#include <tuple>
#include <map>
#include "base/DXH.hpp"

namespace Utils{
	using Microsoft::WRL::ComPtr;

std::tuple<ComPtr<ID3D11Device>, ComPtr<ID3D11DeviceContext>, ComPtr<IDXGISwapChain> > CreateD3DDeviceAndtSwapChain(const HWND winId, const int width, const int height);

UINT GetD3DMSAAQuality(const ComPtr<ID3D11Device> pDevice);

ComPtr<ID3D11RenderTargetView> CreateD3DRenderTargetView(const ComPtr<IDXGISwapChain> pDxgiSwapChain, const ComPtr<ID3D11Device> pDevice) ;

std::pair<ComPtr< ID3D11Texture2D>, ComPtr<ID3D11DepthStencilView>> D3DCreateRenderTexture(const ComPtr<ID3D11Device> pDevice, const UINT quality, const int width, const int height, const bool enableMssaa);

void D3DSetupViewPort(ComPtr<ID3D11DeviceContext> pD3dContext, const int width, const int height);
}

