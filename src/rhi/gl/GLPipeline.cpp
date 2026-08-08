#include "GLPipeline.hpp"
#include <glm/glm.hpp>

namespace rhi {

void GLPipeline::use() {
    if (_shader) glUseProgram(_shader->id());
}

void* GLPipeline::handle() {
    return _shader ? reinterpret_cast<void*>(static_cast<uintptr_t>(_shader->id())) : nullptr;
}

bool GLPipeline::bindShader(const std::shared_ptr<GLShader>& shader, const VertexLayout& layout) {
    _shader = shader;
    _layout = layout.elements;
    if (_vao == 0) glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);
    // 顶点缓冲由 GLRenderer 统一创建；此处记录布局
    return true;
}

void GLPipeline::setDepthTest(bool enable) {
    if (enable) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
}

void GLPipeline::setCullMode(bool enable, int face) {
    if (enable) { glEnable(GL_CULL_FACE); glCullFace(face); }
    else glDisable(GL_CULL_FACE);
}

void GLPipeline::setBlend(bool enable) {
    if (enable) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
}

GLint Locate(const GLShader& s, const std::string& name) {
    return glGetUniformLocation(s.id(), name.c_str());
}

bool GLPipeline::setUniform(const std::string& name, bool value) {
    glUniform1i(Locate(*_shader, name), value); return true;
}
bool GLPipeline::setUniform(const std::string& name, int value) {
    glUniform1i(Locate(*_shader, name), value); return true;
}
bool GLPipeline::setUniform(const std::string& name, float value) {
    glUniform1f(Locate(*_shader, name), value); return true;
}
bool GLPipeline::setUniform(const std::string& name, const float* value, int count) {
    glUniformMatrix4fv(Locate(*_shader, name), count, GL_FALSE, value); return true;
}
bool GLPipeline::setUniform(const std::string& name, const float* value, int count, int vecSize) {
    if (vecSize == 3) glUniform3fv(Locate(*_shader, name), count, value);
    else if (vecSize == 4) glUniform4fv(Locate(*_shader, name), count, value);
    else if (vecSize == 2) glUniform2fv(Locate(*_shader, name), count, value);
    else glUniform1fv(Locate(*_shader, name), count, value);
    return true;
}

} // namespace rhi
