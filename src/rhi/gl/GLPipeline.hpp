#pragma once
#include "rhi/core/IPipeline.hpp"
#include "GLShader.hpp"
#include <string>

namespace rhi {

class GLPipeline : public IPipeline {
public:
    void use() override;
    void* handle() override;
    void setDepthTest(bool enable) override;
    void setCullMode(bool enable, int face) override;
    void setBlend(bool enable) override;
    bool setUniform(const std::string& name, bool value) override;
    bool setUniform(const std::string& name, int value) override;
    bool setUniform(const std::string& name, float value) override;
    bool setUniform(const std::string& name, const float* value, int count) override;
    bool setUniform(const std::string& name, const float* value, int count, int vecSize) override;

    bool bindShader(const std::shared_ptr<GLShader>& shader, const VertexLayout& layout);

private:
    std::shared_ptr<GLShader> _shader{};
    GLuint _vao{0};
    GLuint _vbo{0};
    std::vector<VertexElement> _layout{};
};

} // namespace rhi
