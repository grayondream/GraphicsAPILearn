#include "GLShader.hpp"
#include "utils/FileUtils.hpp"

namespace rhi {

static GLenum ToGLType(ShaderStage::Type type) {
    switch (type) {
        case ShaderStage::Vertex:   return GL_VERTEX_SHADER;
        case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
        case ShaderStage::Geometry: return GL_GEOMETRY_SHADER;
    }
    return GL_VERTEX_SHADER;
}

GLuint GLShader::compileStage(const ShaderStage& stage) {
    const auto src = FileUtils::readFile2String(stage.source);
    GLuint s = glCreateShader(ToGLType(stage.type));
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    int ok{};
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512]{};
        glGetShaderInfoLog(s, 512, nullptr, buf);
        _log += buf;
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool GLShader::compile(const std::vector<ShaderStage>& stages) {
    _log.clear();
    GLuint vs = 0, fs = 0, gs = 0;
    for (const auto& st : stages) {
        auto id = compileStage(st);
        if (id == 0) {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            if (gs) glDeleteShader(gs);
            return false;
        }
        if (st.type == ShaderStage::Vertex) vs = id;
        else if (st.type == ShaderStage::Fragment) fs = id;
        else if (st.type == ShaderStage::Geometry) gs = id;
    }
    _program = glCreateProgram();
    if (vs) glAttachShader(_program, vs);
    if (fs) glAttachShader(_program, fs);
    if (gs) glAttachShader(_program, gs);
    glLinkProgram(_program);
    int ok{};
    glGetProgramiv(_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512]{};
        glGetProgramInfoLog(_program, 512, nullptr, buf);
        _log += buf;
        glDeleteProgram(_program);
        _program = 0;
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        if (gs) glDeleteShader(gs);
        return false;
    }
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    if (gs) glDeleteShader(gs);
    return true;
}

} // namespace rhi
