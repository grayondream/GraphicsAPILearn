#include "GLAppUtils.hpp"
#include "Config/StaticCollector.hpp"
#include "EH/ErrorHandle.hpp"

namespace GLUtils {

    GLProgram CompileShader(const std::string cat, const std::string app, const std::string name) {
        const auto vfile = StaticCollector::getGLShaderPath() / cat / app / (name + ".vert");
        const auto ffile = StaticCollector::getGLShaderPath() / cat / app / (name + ".frag");
        GLProgram program;
        auto ret = program.init(vfile.string(), ffile.string());
        ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
        return program;
    }
}