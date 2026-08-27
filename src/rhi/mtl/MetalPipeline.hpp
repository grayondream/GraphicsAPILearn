#pragma once

#include "rhi/core/IPipeline.hpp"
#include "rhi/core/Common.hpp"
#include <memory>
#include <string>

#if defined(__APPLE__)

#import <Metal/Metal.h>

namespace rhi::mtl {

class MetalShader;

class MetalPipeline : public IPipeline {
public:
    explicit MetalPipeline(void* device);
    ~MetalPipeline() override;

    void use() override;
    void* handle() override;

    bool setUniform(const std::string& name, bool value) override;
    bool setUniform(const std::string& name, int value) override;
    bool setUniform(const std::string& name, float value) override;
    bool setUniform(const std::string& name, const float* value, int count) override;
    bool setUniform(const std::string& name, const float* value, int count, int vecSize) override;
    bool setUniformMatrix(const std::string& name, const float* value, int count, int matSize) override;
    void bindUniformBlock(uint32_t binding) override;

    void setDepthTest(bool enable) override;
    void setDepthFunc(CompareFunc func) override;
    void setDepthMask(bool write) override;
    void setStencilTest(bool enable) override;
    void setStencilFunc(CompareFunc func, int ref, unsigned mask) override;
    void setStencilOp(StencilOp sfail, StencilOp dpfail, StencilOp dppass) override;
    void setStencilMask(unsigned mask) override;
    void setBlend(bool enable) override;
    void setBlendFunc(BlendFactor src, BlendFactor dst) override;
    void setCullFaceEnable(bool enable) override;
    void setCullFace(CullFace face) override;
    void setFrontFace(bool ccw) override;
    void setPolygonMode(PolygonMode mode) override;
    void setPointSizeProgramEnable(bool enable) override;
    void setMultisample(bool enable) override;

    void setPrimitiveType(PrimitiveType type) override;
    PrimitiveType primitiveType() const override;

    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer, uint32_t binding) override;
    void setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) override;

    bool bindShader(MetalShader* shader, const VertexLayout& layout);
    void applyRenderEncoder(void* encoder);
    void ensurePipeline(MTLPixelFormat colorFormat, MTLPixelFormat depthFormat);

private:
    uint64_t stateHash() const;

    id<MTLDevice> _device{nil};
    MetalShader* _shader{nullptr};
    VertexLayout _layout{};
    PrimitiveType _primitive{PrimitiveType::TriangleList};

    bool _depthTest{false};
    CompareFunc _depthFunc{CompareFunc::Less};
    bool _depthWrite{true};
    bool _stencilTest{false};
    StencilState _stencil{};
    BlendState _blend{};
    bool _cullEnable{false};
    CullFace _cullFace{CullFace::Back};
    bool _frontCCW{true};
    PolygonMode _polygonMode{PolygonMode::Fill};
    bool _pointSizeEnable{false};
    bool _multisample{false};

    id<MTLRenderPipelineState> _pipelineState{nil};
    id<MTLDepthStencilState> _depthStencilState{nil};
    uint64_t _lastStateHash{0};
    MTLPixelFormat _lastColorFormat{MTLPixelFormatInvalid};
    MTLPixelFormat _lastDepthFormat{MTLPixelFormatInvalid};

    MTLVertexDescriptor* _vertexDescriptor{nil};
};

} // namespace rhi::mtl

#else // non-Apple fallback

namespace rhi::mtl {

class MetalPipeline : public IPipeline {
public:
    MetalPipeline() = default;
    ~MetalPipeline() override = default;

    void use() override {}
    void* handle() override { return nullptr; }

    bool setUniform(const std::string&, bool) override { return false; }
    bool setUniform(const std::string&, int) override { return false; }
    bool setUniform(const std::string&, float) override { return false; }
    bool setUniform(const std::string&, const float*, int) override { return false; }
    bool setUniform(const std::string&, const float*, int, int) override { return false; }
    bool setUniformMatrix(const std::string&, const float*, int, int) override { return false; }
    void bindUniformBlock(uint32_t) override {}

    void setDepthTest(bool) override {}
    void setDepthFunc(CompareFunc) override {}
    void setDepthMask(bool) override {}
    void setStencilTest(bool) override {}
    void setStencilFunc(CompareFunc, int, unsigned) override {}
    void setStencilOp(StencilOp, StencilOp, StencilOp) override {}
    void setStencilMask(unsigned) override {}
    void setBlend(bool) override {}
    void setBlendFunc(BlendFactor, BlendFactor) override {}
    void setCullFaceEnable(bool) override {}
    void setCullFace(CullFace) override {}
    void setFrontFace(bool) override {}
    void setPolygonMode(PolygonMode) override {}
    void setPointSizeProgramEnable(bool) override {}
    void setMultisample(bool) override {}

    void setPrimitiveType(PrimitiveType type) override { _primitive = type; }
    PrimitiveType primitiveType() const override { return _primitive; }

    void setVertexBuffer(const std::shared_ptr<IBuffer>&, uint32_t) override {}
    void setIndexBuffer(const std::shared_ptr<IBuffer>&) override {}

    bool bindShader(void*, const VertexLayout&) { return false; }
    void applyRenderEncoder(void*) {}
    void ensurePipeline(int, int) {}

private:
    PrimitiveType _primitive{PrimitiveType::TriangleList};
};

} // namespace rhi::mtl

#endif
