#pragma once

#include "app/GL/Base/GLCameraApp.hpp"
#include "native/GL/GLProgram.hpp"
#include "native/GL/GLSphere.hpp" 
#include "native/GL/GLCube.hpp"
#include <glm/glm.hpp>
#include <vector>

// PBR基础应用类，提供PBR渲染的核心功能
class GLIBLSpecularApp : public GLCameraBaseApp {
public:
    // 构造函数和析构函数
    GLIBLSpecularApp() = default;
    virtual ~GLIBLSpecularApp() override;
    
public:
    // 初始化应用
    virtual bool initApp() override;
       
    // 重写绘制场景函数
    virtual void drawScene(const float dt) override;
    
private:
    void initShapes();
    void loadTexture();
    void compileShader();
    void initFramebuffer();
    void initCaptureViews();
    void createIrradianceMap();
    void createPrefilterMap();
    void createQuadBuffer();
    void renderBeforeLoop() override;

    void renderIrradianceMap();
    
    void renderPerfilterMap();
    void renderBrdfLUT();
    void renderToCubemap();

    void renderQuad();
    void renderCube(GLProgram& program, const glm::mat4& model);
    void renderSphere(GLProgram& program, const glm::mat4& model);

    void renderObjectsAndLights(GLProgram& program, const glm::mat4& view, const glm::mat4& projection);
    void renderBackground(GLProgram& program, const glm::mat4& view, const glm::mat4& projection);

protected:
    // PBR着色器程序
    GLProgram m_cubeMapProgram;
    GLProgram m_program;
    GLProgram m_backgroundProgram;
    GLProgram m_irradianceProgram;
    GLProgram m_prefilterProgram;
    GLProgram m_brdfLUTProgram;

    GLSphere m_sphere;
    GLCube m_cube;

    // PBR材质参数
    float m_roughness = 0.5f;   // 粗糙度
    float m_metallic = 0.0f;    // 金属度
    glm::vec3 m_albedo = glm::vec3(0.5f, 0.5f, 0.5f); // 反照率
    float m_ao = 1.0f;          // 环境光遮蔽
    std::shared_ptr<GLImageTexture2D> m_hdrEnvTexture;

    unsigned int m_captureFBO = 0;
    unsigned int m_captureRBO = 0;
    unsigned int m_envCubemap = 0;
    unsigned int m_irradianceMap = 0;
    unsigned int m_prefilterMap = 0;
    unsigned int m_brdfLUTTexture = 0;
    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;
};

