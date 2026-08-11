#ifndef GL_PBR_BASE_APP_HPP
#define GL_PBR_BASE_APP_HPP

#include "app/GL/Base/GLCameraApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class GLPBRBaseApp : public GLCameraBaseApp {
public:
    GLPBRBaseApp() = default;
    virtual ~GLPBRBaseApp() override;

public:
    virtual bool initApp() override;
    virtual void drawScene(const float dt) override;

private:
    void initShapes();
    void compileShader(const rhi::VertexLayout& layout);

protected:
    void renderSphere(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model);

protected:
    std::shared_ptr<rhi::IPipeline> m_program;
    RhiGeometry::Geometry m_sphere;
    std::shared_ptr<rhi::IBuffer> _sphereVb{};
    std::shared_ptr<rhi::IBuffer> _sphereUv{};
    std::shared_ptr<rhi::IBuffer> _sphereNormal{};
    uint32_t _sphereIndexCount{0};

    float m_roughness = 0.5f;
    float m_metallic = 0.0f;
    glm::vec3 m_albedo = glm::vec3(0.5f, 0.5f, 0.5f);
    float m_ao = 1.0f;

    std::vector<glm::vec3> m_lightPositions;
    std::vector<glm::vec3> m_lightColors;
};

#endif
