#pragma once
#include "DX11App.hpp"
#include <memory>

class DX11ImageTexture2D;
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
    void createSampler();

private:
    ComPtr<ID3D11Buffer> _pvecBuffer{};
    ComPtr<ID3D11Buffer> _pidxBuffer{};
    ComPtr< ID3D11InputLayout> _pd3dLayout{};
    std::shared_ptr< DX11ImageTexture2D> _texture{};
    ComPtr<ID3D11SamplerState> _sampler{};
};