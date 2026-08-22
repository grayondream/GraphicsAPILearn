#ifndef GL_IBL_IRRADIANCE_CONVERSION_APP_HPP
#define GL_IBL_IRRADIANCE_CONVERSION_APP_HPP

#include "app/Samples/Base/CameraApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include "app/Samples/RhiGeometry.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class IBLIrradianceConversionApp : public CameraBaseApp {
public:
    virtual ~IBLIrradianceConversionApp();

public:
    virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
    virtual void draw(const float dt) override;

private:
    void initShapes();
    void compileShader(const rhi::VertexLayout& cubeLayout);
    void initFramebuffer();
    void initCaptureViews();
    void loadTexture();
    void renderToCubemap();
    void renderBeforeLoop();
    void renderCube(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model);
    void renderSphere(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model);
    void renderBackground(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection);
    void renderObjectsAndLights(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection);

private:
    RhiGeometry::Geometry m_cube;
    RhiGeometry::Geometry m_sphere;
    std::shared_ptr<rhi::IPipeline> m_program{};
    std::shared_ptr<rhi::IPipeline> m_cubeMapProgram{};
    std::shared_ptr<rhi::IPipeline> m_backgroundProgram{};
    std::shared_ptr<rhi::ITexture2D> m_hdrEnvTexture{};
    std::shared_ptr<rhi::ITexture3D> m_envCubemap{};
    std::shared_ptr<rhi::IRenderTarget> m_captureRT{};
    rhi::UniformBlock _ubo{};
    std::shared_ptr<rhi::IBuffer> _uboBuffer{};
    float m_roughness = 0.5f;
    float m_metallic = 0.0f;
    glm::vec3 m_albedo = glm::vec3(0.5f, 0.5f, 0.5f);
    float m_ao = 1.0f;
    std::vector<glm::vec3> m_lightPositions;
    std::vector<glm::vec3> m_lightColors;
};

#endif
