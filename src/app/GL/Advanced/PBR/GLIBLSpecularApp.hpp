#ifndef GL_IBL_SPECULAR_APP_HPP
#define GL_IBL_SPECULAR_APP_HPP

#include "app/GL/Base/GLCameraApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class GLIBLSpecularApp : public GLCameraBaseApp {
public:
    virtual ~GLIBLSpecularApp();

public:
    virtual bool initApp() override;
    virtual void drawScene(const float dt) override;

private:
    void initShapes();
    void compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout);
    void initFramebuffer();
    void initCaptureViews();
    void createIrradianceMap();
    void createPrefilterMap();
    void loadTexture();
    void renderToCubemap();
    void renderIrradianceMap();
    void renderPerfilterMap();
    void renderBrdfLUT();
    void createBrdfLUT();
    void renderBeforeLoop();
    void renderCube(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model);
    void renderSphere(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model);
    void renderQuad();
    void renderBackground(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection);
    void renderObjectsAndLights(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection);

private:
    RhiGeometry::Geometry m_cube;
    RhiGeometry::Geometry m_sphere;
    RhiGeometry::Geometry m_quad;
    std::shared_ptr<rhi::IPipeline> m_program{};
    std::shared_ptr<rhi::IPipeline> m_cubeMapProgram{};
    std::shared_ptr<rhi::IPipeline> m_backgroundProgram{};
    std::shared_ptr<rhi::IPipeline> m_irradianceProgram{};
    std::shared_ptr<rhi::IPipeline> m_prefilterProgram{};
    std::shared_ptr<rhi::IPipeline> m_brdfLUTProgram{};
    std::shared_ptr<rhi::ITexture2D> m_hdrEnvTexture{};
    std::shared_ptr<rhi::ITexture3D> m_envCubemap{};
    std::shared_ptr<rhi::ITexture3D> m_irradianceMap{};
    std::shared_ptr<rhi::ITexture3D> m_prefilterMap{};
    std::shared_ptr<rhi::IRenderTarget> m_captureRT{};
    std::shared_ptr<rhi::IRenderTarget> m_brdfLUTRT{};
    std::shared_ptr<rhi::ITexture2D> m_brdfLUTTexture{};
    float m_roughness = 0.5f;
    float m_metallic = 0.0f;
    glm::vec3 m_albedo = glm::vec3(0.5f, 0.5f, 0.5f);
    float m_ao = 1.0f;
    std::vector<glm::vec3> m_lightPositions;
    std::vector<glm::vec3> m_lightColors;
};

#endif
