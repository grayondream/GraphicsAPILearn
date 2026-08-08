#pragma once
#include <string>
#include <native/GL/GLProgram.hpp>

namespace GLUtils {
    GLProgram CompileShader(const std::string cat, const std::string app, const std::string name);
}