#pragma once
#include "DX11App.hpp"

class DX11TriangleApp : public DX11App {
public:
    DX11TriangleApp();
    virtual ~DX11TriangleApp();

public:
    virtual bool init(const HINSTANCE, const WindowDesc& param) override;

private:
    void createGemBuffer();
    void compileShader();
    
    virtual void updateScene(const float dt) override;
    virtual void drawScene() override;

private:
    ComPtr<ID3D11Buffer> _pvecBuffer{};
    ID3DX11EffectMatrixVariable* _pd3dWVP{};
    ComPtr< ID3D11InputLayout> _pd3dLayout{};
};