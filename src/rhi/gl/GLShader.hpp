#pragma once
#include "rhi/core/IShader.hpp"
#include "GLHeader.hpp"
#include <string>

namespace rhi {

class GLShader : public IShader {
public:
    bool compile(const std::vector<ShaderStage>& stages) override;
    std::string getLog() const override { return _log; }
    bool valid() const override { return _program != 0; }
    GLuint id() const { return _program; }

private:
    GLuint compileStage(const ShaderStage& stage);
    GLuint _program{0};
    std::string _log{};
};

} // namespace rhi
