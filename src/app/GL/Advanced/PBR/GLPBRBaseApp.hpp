#ifndef GL_PBR_BASE_APP_HPP
#define GL_PBR_BASE_APP_HPP

#include "App/GL/Base/GLCameraApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Native/GL/GLSphere.hpp" // 假设存在球体类
#include <glm/glm.hpp>
#include <vector>

// PBR基础应用类，提供PBR渲染的核心功能
class GLPBRBaseApp : public GLCameraBaseApp {
public:
    // 构造函数和析构函数
    GLPBRBaseApp() = default;
    virtual ~GLPBRBaseApp() override;
    
public:
    // 初始化应用
    virtual bool initApp() override;
       
    // 重写绘制场景函数
    virtual void drawScene(const float dt) override;
    
private:
    void initShapes();
    void compileShader();
    void renderSphere(GLProgram& program, const glm::mat4& model);

protected:
    // PBR着色器程序
    GLProgram m_program;
    
    // 球体几何体（用于渲染PBR物体和光源）
    GLSphere m_sphere;
    
    // PBR材质参数
    float m_roughness = 0.5f;   // 粗糙度
    float m_metallic = 0.0f;    // 金属度
    glm::vec3 m_albedo = glm::vec3(0.5f, 0.5f, 0.5f); // 反照率
    float m_ao = 1.0f;          // 环境光遮蔽
    
    // 光源数据
    std::vector<glm::vec3> m_lightPositions;
    std::vector<glm::vec3> m_lightColors;
};

#endif // GL_PBR_BASE_APP_HPP