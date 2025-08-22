#pragma once

#include "App/GL/Base/GLCameraApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Native/GL/GLSphere.hpp" 
#include "Native/GL/GLCube.hpp"
#include <glm/glm.hpp>
#include <vector>

// PBR基础应用类，提供PBR渲染的核心功能
class GLPBRTextureApp : public GLCameraBaseApp {
public:
    // 构造函数和析构函数
    GLPBRTextureApp() = default;
    virtual ~GLPBRTextureApp() override;
    
public:
    // 初始化应用
    virtual bool initApp() override;
       
    // 重写绘制场景函数
    virtual void drawScene(const float dt) override;
    
private:
    void initShapes();
    void loadTexture();
    void compileShader();
    void renderSphere(GLProgram& program, const glm::mat4& model);

protected:
    // PBR着色器程序
    GLProgram m_program;
    
    // 球体几何体（用于渲染PBR物体和光源）
    GLSphere m_sphere;
    
    std::shared_ptr<GLImageTexture2D> m_albedoMap;
    std::shared_ptr<GLImageTexture2D> m_roughnessMap;
    std::shared_ptr<GLImageTexture2D> m_metallicMap;
    std::shared_ptr<GLImageTexture2D> m_aoMap;
    std::shared_ptr<GLImageTexture2D> m_normalMap;

};

