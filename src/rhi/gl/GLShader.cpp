#include "GLShader.hpp"
#include "utils/FileUtils.hpp"

#include <cstdio>
#include <regex>

namespace rhi {

namespace {
// 部分平台（如 macOS）OpenGL 仅到 4.1，对应 GLSL 410；
// 项目着色器以 #version 430 + layout(binding=...) 编写，需在低于 4.3 的驱动上向下归一化，
// 否则编译直接失败。归一化不改变 Linux/Windows（GLSL>=4.30）行为。
bool needGLSLDowngrade() {
    const char* ver = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    if (!ver) return false;
    int major = 0, minor = 0;
    std::sscanf(ver, "%d.%d", &major, &minor);
    return (major * 100 + minor) < 430;
}

// 将 layout(binding = N) 等限定符从 layout(...) 中剥离（保留 location 等），
// 若剥离后 layout 内容为空则整体删除该 layout 限定符。
std::string stripLayoutBindings(std::string src) {
    static const std::regex layoutRe(R"(layout\s*\(\s*([^)]*)\))");
    std::string out;
    size_t last = 0;
    for (std::sregex_iterator it(src.begin(), src.end(), layoutRe); it != std::sregex_iterator(); ++it) {
        const auto& m = *it;
        out += src.substr(last, m.position() - last);
        std::string inner = m[1].str();
        inner = std::regex_replace(inner, std::regex(R"(\bbinding\s*=\s*\d+)"), "");
        inner = std::regex_replace(inner, std::regex(R"(^\s*,\s*)"), "");
        inner = std::regex_replace(inner, std::regex(R"(,\s*$)"), "");
        inner = std::regex_replace(inner, std::regex(R"(,\s*,)"), ",");
        inner = std::regex_replace(inner, std::regex(R"(\s{2,})"), " ");
        const size_t a = inner.find_first_not_of(" \t");
        const size_t b = inner.find_last_not_of(" \t");
        if (a != std::string::npos) {
            out += "layout(" + inner.substr(a, b - a + 1) + ")";
        }
        last = m.position() + m.length();
    }
    out += src.substr(last);
    return out;
}

std::string normalizeGLSL(std::string src) {
    src = std::regex_replace(src, std::regex(R"(#version\s+430\b)"), "#version 410");
    src = stripLayoutBindings(std::move(src));
    return src;
}
} // namespace

static GLenum ToGLType(ShaderStage::Type type) {
    switch (type) {
        case ShaderStage::Vertex:   return GL_VERTEX_SHADER;
        case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
        case ShaderStage::Geometry: return GL_GEOMETRY_SHADER;
        case ShaderStage::Compute:  return GL_COMPUTE_SHADER;
    }
    return GL_VERTEX_SHADER;
}

GLuint GLShader::compileStage(const ShaderStage& stage) {
    std::string src = FileUtils::readFile2String(stage.source);
    if (needGLSLDowngrade()) {
        src = normalizeGLSL(std::move(src));
    }
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
    for (const auto& st : stages) {
        if (st.type == ShaderStage::Compute) {
            _log = "Compute shaders are not supported by the GL backend yet";
            return false;
        }
    }
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
    collectUniformBlocks();
    return true;
}

void GLShader::collectUniformBlocks() {
    _blocks.clear();
    if (!_program) return;
    GLint count = 0;
    glGetProgramiv(_program, GL_ACTIVE_UNIFORM_BLOCKS, &count);
    for (GLint i = 0; i < count; ++i) {
        GLsizei len = 0;
        glGetActiveUniformBlockiv(_program, i, GL_UNIFORM_BLOCK_NAME_LENGTH, &len);
        std::string name(len > 0 ? static_cast<size_t>(len) : 1u, '\0');
        glGetActiveUniformBlockName(_program, i, len, nullptr, &name[0]);
        if (!name.empty() && name.back() == '\0') name.pop_back();
        _blocks[name] = static_cast<GLuint>(i);
    }
}

} // namespace rhi
