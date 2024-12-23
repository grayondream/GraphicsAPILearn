#pragma once
#include "Application.hpp"

class TriangleApplication : public Application {
public:
    TriangleApplication();
    virtual ~TriangleApplication();

public:
    virtual bool init(const HINSTANCE, const CreateParam param) override;

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