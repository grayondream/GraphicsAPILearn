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

// ---- 渲染状态命令全集 ----

static GLenum ToGLCompare(CompareFunc f) {
    switch (f) {
        case CompareFunc::Never:        return GL_NEVER;
        case CompareFunc::Less:         return GL_LESS;
        case CompareFunc::Equal:        return GL_EQUAL;
        case CompareFunc::LessEqual:    return GL_LEQUAL;
        case CompareFunc::Greater:      return GL_GREATER;
        case CompareFunc::NotEqual:     return GL_NOTEQUAL;
        case CompareFunc::GreaterEqual: return GL_GEQUAL;
        case CompareFunc::Always:       return GL_ALWAYS;
    }
    return GL_ALWAYS;
}

static GLenum ToGLStencilOp(StencilOp op) {
    switch (op) {
        case StencilOp::Keep:      return GL_KEEP;
        case StencilOp::Zero:      return GL_ZERO;
        case StencilOp::Replace:   return GL_REPLACE;
        case StencilOp::Incr:      return GL_INCR;
        case StencilOp::Decr:      return GL_DECR;
        case StencilOp::IncrWrap:  return GL_INCR_WRAP;
        case StencilOp::DecrWrap:  return GL_DECR_WRAP;
    }
    return GL_KEEP;
}

static GLenum ToGLBlendFactor(BlendFactor f) {
    switch (f) {
        case BlendFactor::Zero:              return GL_ZERO;
        case BlendFactor::One:               return GL_ONE;
        case BlendFactor::SrcAlpha:          return GL_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:  return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::SrcColor:          return GL_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:  return GL_ONE_MINUS_SRC_COLOR;
    }
    return GL_SRC_ALPHA;
}

static GLenum ToGLPolygonMode(PolygonMode m) {
    switch (m) {
        case PolygonMode::Fill:  return GL_FILL;
        case PolygonMode::Line:  return GL_LINE;
        case PolygonMode::Point: return GL_POINT;
    }
    return GL_FILL;
}

static GLenum ToGLFace(CullFace f) {
    switch (f) {
        case CullFace::Back:         return GL_BACK;
        case CullFace::Front:        return GL_FRONT;
        case CullFace::FrontAndBack: return GL_FRONT_AND_BACK;
    }
    return GL_BACK;
}

void GLPipeline::bindUniformBlock(uint32_t binding) {
    if (!_shader) return;
    for (const auto& [name, index] : _shader->uniformBlocks()) {
        (void)name;
        glUniformBlockBinding(_shader->id(), index, binding);
    }
}

void GLPipeline::setDepthFunc(CompareFunc func) { glDepthFunc(ToGLCompare(func)); }
void GLPipeline::setDepthMask(bool write) { glDepthMask(write ? GL_TRUE : GL_FALSE); }

void GLPipeline::setStencilTest(bool enable) {
    if (enable) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
}

void GLPipeline::setStencilFunc(CompareFunc func, int ref, unsigned mask) {
    glStencilFunc(ToGLCompare(func), ref, mask);
}

void GLPipeline::setStencilOp(StencilOp sfail, StencilOp dpfail, StencilOp dppass) {
    glStencilOp(ToGLStencilOp(sfail), ToGLStencilOp(dpfail), ToGLStencilOp(dppass));
}

void GLPipeline::setStencilMask(unsigned mask) { glStencilMask(mask); }

void GLPipeline::setBlendFunc(BlendFactor src, BlendFactor dst) {
    glBlendFunc(ToGLBlendFactor(src), ToGLBlendFactor(dst));
}

void GLPipeline::setCullFaceEnable(bool enable) {
    if (enable) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
}

void GLPipeline::setCullFace(CullFace face) { glCullFace(ToGLFace(face)); }
void GLPipeline::setFrontFace(bool ccw) { glFrontFace(ccw ? GL_CCW : GL_CW); }

void GLPipeline::setPolygonMode(PolygonMode mode) {
    glPolygonMode(GL_FRONT_AND_BACK, ToGLPolygonMode(mode));
}

void GLPipeline::setMultisample(bool enable) {
    if (enable) glEnable(GL_MULTISAMPLE); else glDisable(GL_MULTISAMPLE);
}

} // namespace rhi
