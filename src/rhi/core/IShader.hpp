#pragma once
#include <string>
#include <vector>
#include "Common.hpp"

namespace rhi {

class IShader {
public:
    virtual ~IShader() = default;
    virtual bool compile(const std::vector<ShaderStage>& stages) = 0;
    virtual std::string getLog() const = 0;  // 编译失败时的后端日志
    virtual bool valid() const = 0;
};

} // namespace rhi
