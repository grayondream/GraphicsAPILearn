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

    // 显式 UBO：声明本 pipeline 绑定的 uniform block 槽位
    virtual void bindUniformBlock(uint32_t binding) = 0;

    // 渲染状态（显式暴露以便学习对比各 API 差异）
    virtual void setDepthTest(bool enable) = 0;
    virtual void setCullMode(bool enable, int face) = 0;
    virtual void setBlend(bool enable) = 0;

    // 渲染状态命令全集（新增）
    virtual void setDepthFunc(CompareFunc func) = 0;
    virtual void setDepthMask(bool write) = 0;
    virtual void setStencilTest(bool enable) = 0;
    virtual void setStencilFunc(CompareFunc func, int ref, unsigned mask) = 0;
    virtual void setStencilOp(StencilOp sfail, StencilOp dpfail, StencilOp dppass) = 0;
    virtual void setStencilMask(unsigned mask) = 0;
    virtual void setBlendFunc(BlendFactor src, BlendFactor dst) = 0;
    virtual void setCullFaceEnable(bool enable) = 0;
    virtual void setCullFace(CullFace face) = 0;
    virtual void setFrontFace(bool ccw) = 0;               // true=CCW 正面，false=CW
    virtual void setPolygonMode(PolygonMode mode) = 0;
    virtual void setMultisample(bool enable) = 0;
};

} // namespace rhi
