#include "DX11Texture2D.hpp"
#include "glad/glad.h"
#include <cassert>
#include <d3d11.h>
#include <wrl/client.h> 

DX11Texture2D::~DX11Texture2D() {
	release();
}

bool DX11Texture2D::init(const Texture2DDataView& data) {
	D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = data.size().width;
    textureDesc.Height = data.size().height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  
    textureDesc.SampleDesc.Count = 1; 
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;  
    textureDesc.CPUAccessFlags = 0;
    auto ret = _pDevice->CreateTexture2D(&textureDesc, nullptr, _ptexture.GetAddressOf());
    if (FAILED(ret)) {
        return false;
    }

    ret = _pDevice->CreateShaderResourceView(_ptexture.Get(), nullptr, _pshaderView.GetAddressOf());
    return !FAILED(ret);
}

void DX11Texture2D::bind(const unsigned int unit) {
    _pContext->PSSetShaderResources(unit, 1, _pshaderView.GetAddressOf());
}

bool DX11Texture2D::init(const std::string& file) {
    const auto ret = D3DX11CreateShaderResourceViewFromFile(_pDevice.Get(), file.c_str(), nullptr, nullptr, _pshaderView.GetAddressOf(), nullptr);
    return !FAILED(ret);
}

void DX11Texture2D::release() {
    if (_ptexture) {
        _ptexture->Release();
        _ptexture = nullptr;
    }
}

void* DX11Texture2D::handle() {
    return reinterpret_cast<void*>(_ptexture.Get());
}

bool DX11Texture2D::valid() {
    return _ptexture || _pshaderView;
}