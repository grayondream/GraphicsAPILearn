#pragma once
#include "DX11App.hpp"

class DX11SimpleTextureApp : public DX11App {
public:
    DX11SimpleTextureApp();
    virtual ~DX11SimpleTextureApp();

public:
    virtual bool init(const HINSTANCE, const WindowDesc& param) override;

private:
    void createGemBuffer();
    void compileShader();
    
    virtual void updateScene(const float dt) override;
    virtual void drawScene() override;

private:
    ComPtr<ID3D11Buffer> _pvecBuffer{};
    ComPtr<ID3D11Buffer> _pidxBuffer{};
    ID3DX11EffectMatrixVariable* _pd3dWVP{};
    ComPtr< ID3D11InputLayout> _pd3dLayout{};
};