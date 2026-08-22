#ifndef GL_PBR_TEXTURE_APP_HPP
#define GL_PBR_TEXTURE_APP_HPP

#include "PBRBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>

class PBRTextureApp : public PBRBaseApp {
public:
    virtual ~PBRTextureApp();

public:
    virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
    virtual void draw(const float dt) override;

private:
    void loadTexture();
    void compileShader();

private:
    std::shared_ptr<rhi::ITexture2D> m_albedoMap{};
    std::shared_ptr<rhi::ITexture2D> m_roughnessMap{};
    std::shared_ptr<rhi::ITexture2D> m_metallicMap{};
    std::shared_ptr<rhi::ITexture2D> m_aoMap{};
    std::shared_ptr<rhi::ITexture2D> m_normalMap{};
};

#endif
