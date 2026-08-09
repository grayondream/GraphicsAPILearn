#pragma once
#include "rhi/core/IShader.hpp"
#include "GLHeader.hpp"
#include <string>
#include <unordered_map>

namespace rhi {

class GLShader : public IShader {
public:
    bool compile(const std::vector<ShaderStage>& stages) override;
    std::string getLog() const override { return _log; }
    bool valid() const override { return _program != 0; }
    GLuint id() const { return _program; }

    const std::unordered_map<std::string, GLuint>& uniformBlocks() const { return _blocks; }
    void collectUniformBlocks();   // compile 成功后调用

private:
    GLuint compileStage(const ShaderStage& stage);
    GLuint _program{0};
    std::string _log{};
    std::unordered_map<std::string, GLuint> _blocks{};
};

} // namespace rhi
