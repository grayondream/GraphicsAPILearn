#pragma once
#include "DX11App.hpp"

class BoxApplication : public DX11Application {
public:
    struct Rotation {
        float theta{};
        float phi{};
        float radius{};
    };

    struct MVPMatrix {
        XMFLOAT4X4 world{};
        XMFLOAT4X4 view{};
        XMFLOAT4X4 proj{};
    };
public:
    BoxApplication();
    virtual ~BoxApplication();

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
    ComPtr<ID3D11Buffer> _pmvpBuffer{};
    ID3DX11EffectMatrixVariable* _pd3dWVP{};
    ComPtr< ID3D11InputLayout> _pd3dLayout{};
    MVPMatrix _mvp{};
    Rotation _rotation{};
    POINT _lastMousePt{};
};