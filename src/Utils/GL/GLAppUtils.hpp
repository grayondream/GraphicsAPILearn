#pragma once
#include <string>
#include <Native/GL/GLProgram.hpp>

namespace GLUtils {
    GLProgram CompileShader(const std::string name);
}