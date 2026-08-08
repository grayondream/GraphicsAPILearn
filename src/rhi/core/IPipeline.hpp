#pragma once
#include "Common.hpp"
#include <memory>

namespace rhi {

class IShader;

class IPipeline {
public:
    virtual ~IPipeline() = default;
    virtual void use() = 0;
    virtual void* handle() = 0;

    virtual bool setUniform(const std::string& name, bool value) = 0;
    virtual bool setUniform(const std::string& name, int value) = 0;
    virtual bool setUniform(const std::string& name, float value) = 0;
    virtual bool setUniform(const std::string& name, const float* value, int count) = 0;   // 矩阵/数组
    virtual bool setUniform(const std::string& name, const float* value, int count, int vecSize) = 0;

    // 渲染状态（显式暴露以便学习对比各 API 差异）
    virtual void setDepthTest(bool enable) = 0;
    virtual void setCullMode(bool enable, int face) = 0;
    virtual void setBlend(bool enable) = 0;
};

} // namespace rhi
