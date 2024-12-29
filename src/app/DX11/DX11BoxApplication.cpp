#include "DX11BoxApplication.hpp"
#include "Base/Vertex.hpp"
#include "Base/DXBaseConexpr.hpp"
#include "EH/ErrorHandle.hpp"
#include "Config/StaticCollectorPredefined.hpp"
#include <Base/MathHelper.h>
#include <filesystem>

namespace eh = ErrorHandle;
namespace fs = std::filesystem;
namespace sc = StaticCollector;

BoxApplication::BoxApplication() {
    _rotation.phi = 0.25 * MathHelper::Pi;
    _rotation.radius = 0.5;
    _rotation.theta = 1.5 * MathHelper::Pi;

    XMMATRIX id = XMMatrixIdentity();
    XMStoreFloat4x4(&_mvp.proj, id);
    XMStoreFloat4x4(&_mvp.view, id);
    XMStoreFloat4x4(&_mvp.world, id);
}

BoxApplication::~BoxApplication(){
    
}

void BoxApplication::createGemBuffer() {
    Vertex vecs[] =
    {
        { XMFLOAT3(-1.0f, -1.0f, -1.0f), (const float*)&Colors::White   },
        { XMFLOAT3(-1.0f, +1.0f, -1.0f), (const float*)&Colors::Black   },
        { XMFLOAT3(+1.0f, +1.0f, -1.0f), (const float*)&Colors::Red     },
        { XMFLOAT3(+1.0f, -1.0f, -1.0f), (const float*)&Colors::Green   },
        { XMFLOAT3(-1.0f, -1.0f, +1.0f), (const float*)&Colors::Blue    },
        { XMFLOAT3(-1.0f, +1.0f, +1.0f), (const float*)&Colors::Yellow  },
        { XMFLOAT3(+1.0f, +1.0f, +1.0f), (const float*)&Colors::Cyan    },
        { XMFLOAT3(+1.0f, -1.0f, +1.0f), (const float*)&Colors::Magenta }
    };

    D3D11_BUFFER_DESC vecDesc{};
    vecDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vecDesc.ByteWidth = sizeof(vecs);
    vecDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = vecs;
    eh::ExitIfFailed(_pd3dDevice->CreateBuffer(&vecDesc, &data, _pvecBuffer.GetAddressOf()), "Failed to create box vectrics buffer");

    UINT indices[] = {
        // front face
        0, 1, 2,
        0, 2, 3,

        // back face
        4, 6, 5,
        4, 7, 6,

        // left face
        4, 5, 1,
        4, 1, 0,

        // right face
        3, 2, 6,
        3, 6, 7,

        // top face
        1, 5, 6,
        1, 6, 2,

        // bottom face
        4, 0, 3,
        4, 3, 7
    };

    D3D11_BUFFER_DESC indDesc{};
    indDesc.Usage = D3D11_USAGE_IMMUTABLE;
    indDesc.ByteWidth = sizeof(indices);
    indDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA idata{};
    idata.pSysMem = indices;
    eh::ExitIfFailed(_pd3dDevice->CreateBuffer(&indDesc, &data, _pidxBuffer.GetAddressOf()), "Failed to create box index buffer");

    D3D11_BUFFER_DESC constBufferDesc{};
    constBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    constBufferDesc.ByteWidth = sizeof(XMFLOAT4X4);
    constBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    eh::ExitIfFailed(_pd3dDevice->CreateBuffer(&constBufferDesc, NULL, _pmvpBuffer.GetAddressOf()), "Failed to create box mvp buffer");
}

void BoxApplication::compileShader() {
    DWORD sharedFlags{};

    ComPtr<ID3D10Blob> pvsShader{}, pfsShader{}, pErrorBlob{};
    const fs::path cfgPath(std::wstring(kResourceRoot));
    const fs::path fxfile = cfgPath / L"shape" / L"box.hlsl";
    auto hr = D3DX11CompileFromFileW(fxfile.wstring().c_str(), 0, 0, "vs_main", "vs_4_0", sharedFlags, 0, 0, pvsShader.GetAddressOf(), pErrorBlob.GetAddressOf(), 0);
    if (FAILED(hr) && pErrorBlob) {
        OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
    }

    eh::ExitIfFailed(hr, "Compile VS Shader file failed");
    hr = D3DX11CompileFromFileW(fxfile.wstring().c_str(), 0, 0, "ps_main", "ps_4_0", sharedFlags, 0, 0, pfsShader.GetAddressOf(), pErrorBlob.GetAddressOf(), 0);
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
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    // Create the input layout
    eh::ExitIfFailed(_pd3dDevice->CreateInputLayout(ied, 2, pvsShader->GetBufferPointer(),
        pvsShader->GetBufferSize(), _pd3dLayout.GetAddressOf()),
        "Can not create layout");
    _pd3dDeviceCtx->IASetInputLayout(_pd3dLayout.Get());
}

bool BoxApplication::init(const HINSTANCE ins, const WindowDesc& param) {
    if (!DX11Application::init(ins, param)) {
        return false;
    }

    compileShader();
    createGemBuffer();
    return true;
}

void BoxApplication::updateScene(const float dt) {
    D3D11_MAPPED_SUBRESOURCE mappedSubresource;
    _pd3dDeviceCtx->Map(_pvecBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
    XMFLOAT4X4* pmvp = (XMFLOAT4X4*)(mappedSubresource.pData);
    *pmvp = _mvp.proj;
    _pd3dDeviceCtx->Unmap(_pvecBuffer.Get(), 0);
}

void BoxApplication::drawScene() {
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    auto* buffer = _pvecBuffer.Get();
    _pd3dDeviceCtx->VSSetConstantBuffers(0, 1, _pmvpBuffer.GetAddressOf());
    _pd3dDeviceCtx->IASetVertexBuffers(0, 1, _pvecBuffer.GetAddressOf(), &stride, &offset);
    _pd3dDeviceCtx->IASetIndexBuffer(_pidxBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    _pd3dDeviceCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    _pd3dDeviceCtx->DrawIndexed(36, 0, 0);
}



