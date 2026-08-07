#include "GLAppUtils.hpp"
#include "Base/StaticCollector.hpp"
#include "Base/ErrorHandle.hpp"
#include <Utils/FileUtils.hpp>
using FileUtils::join;
namespace GLUtils {

    GLProgram CompileShader(const std::string cat, const std::string app, const std::string name) {
        const auto vfile = join(StaticCollector::getGLShaderPath(), cat, app, (name + ".vert"));
        const auto ffile = join(StaticCollector::getGLShaderPath(), cat, app, (name + ".frag"));
        GLProgram program;
        auto ret = program.init(vfile, ffile);
        ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
        return program;
    }
}