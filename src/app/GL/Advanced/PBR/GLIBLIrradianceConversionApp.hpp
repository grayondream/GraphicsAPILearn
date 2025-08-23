#pragma once

#include "App/GL/Base/GLCameraApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Native/GL/GLSphere.hpp" 
#include "Native/GL/GLCube.hpp"
#include <glm/glm.hpp>
#include <vector>

// PBR基础应用类，提供PBR渲染的核心功能
class GLIBLIrradianceConversionApp : public GLCameraBaseApp {
public:
    // 构造函数和析构函数
    GLIBLIrradianceConversionApp() = default;
    virtual ~GLIBLIrradianceConversionApp() override;
    
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
    void renderBeforeLoop() override;

    void renderToCubemap();

    void renderCube(GLProgram& program, const glm::mat4& model);
    void renderSphere(GLProgram& program, const glm::mat4& model);

    void renderObjectsAndLights(GLProgram& program, const glm::mat4& view, const glm::mat4& projection);
    void renderBackground(GLProgram& program, const glm::mat4& view, const glm::mat4& projection);

protected:
    // PBR着色器程序
    GLProgram m_cubeMapProgram;
    GLProgram m_program;
    GLProgram m_backgroundProgram;
    
    GLSphere m_sphere;
    GLCube m_cube;

    

    std::shared_ptr<GLImageTexture2D> m_albedoMap;
    std::shared_ptr<GLImageTexture2D> m_roughnessMap;
    std::shared_ptr<GLImageTexture2D> m_metallicMap;
    std::shared_ptr<GLImageTexture2D> m_aoMap;
    std::shared_ptr<GLImageTexture2D> m_normalMap;
    std::shared_ptr<GLImageTexture2D> m_hdrEnvTexture;

    unsigned int m_captureFBO = 0;
    unsigned int m_captureRBO = 0;
    unsigned int m_envCubemap = 0;
};

