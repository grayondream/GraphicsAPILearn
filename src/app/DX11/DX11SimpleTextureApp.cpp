#include "DX11SimpleTextureApp.hpp"
#include "Base/DXBaseConexpr.hpp"
#include "EH/ErrorHandle.hpp"
#include "Config/StaticCollectorPredefined.hpp"
#include <Base/MathHelper.h>
#include <filesystem>
#include "Geometry//Rect.hpp"
#include "Native/DX11/DX11ImageTexture2D.hpp"

namespace eh = ErrorHandle;
namespace fs = std::filesystem;
namespace sc = StaticCollector;

DX11SimpleTextureApp::DX11SimpleTextureApp() {
}

DX11SimpleTextureApp::~DX11SimpleTextureApp(){
    
}

void DX11SimpleTextureApp::createGemBuffer() {
    Rect shape{};

    D3D11_BUFFER_DESC vecDesc{};
    vecDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vecDesc.ByteWidth = shape.byteSize();
    vecDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = shape.toDX11().data();
    eh::ExitIfFailed(_pd3dDevice->CreateBuffer(&vecDesc, &data, _pvecBuffer.GetAddressOf()), "Failed to create box vectrics buffer");

    D3D11_BUFFER_DESC idxDesc{};
    idxDesc.Usage = D3D11_USAGE_IMMUTABLE;
    idxDesc.ByteWidth = shape.idxByteSize();
    idxDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA idxData{};
    idxData.pSysMem = shape.idx();
    eh::ExitIfFailed(_pd3dDevice->CreateBuffer(&idxDesc, &idxData, _pidxBuffer.GetAddressOf()), "Failed to create box index buffer");

    const auto imgFile = StaticCollector::getImagePath() / "dog.jpg";
    _texture = std::make_shared< DX11ImageTexture2D>(imgFile.string());
    _texture->setContext(_pd3dDeviceCtx, _pd3dDevice);
    eh::ExitIfFailed(_texture->load().texture()->valid(), "Failed to load texture from file");
}

void DX11SimpleTextureApp::createSampler() {
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;  // ѡ�����Թ���
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;      // �����������곬��ʱ�Ĵ�����ʽ
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    // ����������
    eh::ExitIfFailed(_pd3dDevice->CreateSamplerState(&samplerDesc, _sampler.GetAddressOf()), "Failed to create sampler");
}

void DX11SimpleTextureApp::compileShader() {
    DWORD sharedFlags{};

    ComPtr<ID3D10Blob> pvsShader{}, pfsShader{}, pErrorBlob{};
    const fs::path file = StaticCollector::getDX11ShaderPath() / "Shape" / "simpleTexture.hlsl";
    auto hr = D3DX11CompileFromFileW(file.wstring().c_str(), 0, 0, "vs_main", "vs_4_0", sharedFlags, 0, 0, pvsShader.GetAddressOf(), pErrorBlob.GetAddressOf(), 0);
    if (FAILED(hr) && pErrorBlob) {
        OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
    }

    eh::ExitIfFailed(hr, "Compile VS Shader file failed");
    hr = D3DX11CompileFromFileW(file.wstring().c_str(), 0, 0, "ps_main", "ps_4_0", sharedFlags, 0, 0, pfsShader.GetAddressOf(), pErrorBlob.GetAddressOf(), 0);
    if (FAILED(hr) && pErrorBlob) {
        OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
    }

    eh::ExitIfFailed(hr, "Compile FS Shader file failed");

    ComPtr<ID3D11VertexShader> pvShader{};
    ComPtr<ID3D11PixelShader> pfShader{};
    hr = _pd3dDevice->CreateVertexShader(pvsShader->GetBufferPointer(), pvsShader->GetBufferSize(), 0, pvShader.GetAddressOf());
    eh::ExitIfFailed(hr, "Create VS file failed");
    hr = _pd3dDevice->CreatePixelShader(pfsShader->GetBufferPointer(), pfsShader->GetBufferSize(), 0, pfShader.GetAddressOf());
    eh::ExitIfFailed(hr, "Create FS file failed");
    
    _pd3dDeviceCtx->VSSetShader(pvShader.Get(), 0, 0);
    _pd3dDeviceCtx->PSSetShader(pfShader.Get(), 0, 0);

    // Create the vertex input layout.
    D3D11_INPUT_ELEMENT_DESC ied[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD0", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    // Create the input layout
    eh::ExitIfFailed(_pd3dDevice->CreateInputLayout(ied, ARRAYSIZE(ied), pvsShader->GetBufferPointer(),
        pvsShader->GetBufferSize(), _pd3dLayout.GetAddressOf()),
        "Failed to create layout");
    _pd3dDeviceCtx->IASetInputLayout(_pd3dLayout.Get());
}

bool DX11SimpleTextureApp::init(const HINSTANCE ins, const WindowDesc& param) {
    if (!DX11App::init(ins, param)) {
        return false;
    }

    compileShader();
    createGemBuffer();
    createSampler();
    return true;
}

void DX11SimpleTextureApp::drawScene(const float dt) {
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    auto* buffer = _pvecBuffer.Get();
    _pd3dDeviceCtx->PSSetSamplers(0, 1, _sampler.GetAddressOf());
    _texture->texture()->bind(0);
    _pd3dDeviceCtx->IASetVertexBuffers(0, 1, _pvecBuffer.GetAddressOf(), &stride, &offset);
    _pd3dDeviceCtx->IASetIndexBuffer(_pidxBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    _pd3dDeviceCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    _pd3dDeviceCtx->DrawIndexed(6, 0, 0);
    return DX11App::drawScene(dt);
}



