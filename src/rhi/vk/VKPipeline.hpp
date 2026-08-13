#pragma once
#include "VKHeader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "VKShader.hpp"

namespace rhi {

class VKShader;

class VKPipeline : public IPipeline {
public:
    VKPipeline(vk::raii::Device& device, vk::PipelineLayout layout, vk::RenderPass renderPass,
               vk::Format swapchainFormat, VertexLayout layoutIn, std::shared_ptr<VKShader> shader);
    ~VKPipeline() override = default;

    void use() override;
    void* handle() override;

    bool setUniform(const std::string&, bool) override { return false; }
    bool setUniform(const std::string&, int) override { return false; }
    bool setUniform(const std::string&, float) override { return false; }
    bool setUniform(const std::string&, const float*, int) override { return false; }
    bool setUniform(const std::string&, const float*, int, int) override { return false; }
    bool setUniformMatrix(const std::string&, const float*, int, int) override { return false; }
    void bindUniformBlock(uint32_t) override {}

    void setDepthTest(bool) override;
    void setCullMode(bool, int) override;
    void setBlend(bool) override;
    void setDepthFunc(CompareFunc) override;
    void setDepthMask(bool) override;
    void setStencilTest(bool) override;
    void setStencilFunc(CompareFunc, int, unsigned) override;
    void setStencilOp(StencilOp, StencilOp, StencilOp) override;
    void setStencilMask(unsigned) override;
    void setBlendFunc(BlendFactor, BlendFactor) override;
    void setCullFaceEnable(bool) override;
    void setCullFace(CullFace) override;
    void setFrontFace(bool) override;
    void setPolygonMode(PolygonMode) override;
    void setPointSizeProgramEnable(bool enable) override { _pointSizeEnable = enable; }
    void setMultisample(bool) override;
    void setPrimitiveType(PrimitiveType) override;
    PrimitiveType primitiveType() const override { return _primitive; }
    void setVertexBuffer(const std::shared_ptr<IBuffer>&, uint32_t) override {}
    void setIndexBuffer(const std::shared_ptr<IBuffer>&) override {}

    bool ensureCreated();
    void applyDynamicState(vk::raii::CommandBuffer& cmd) const;
    vk::Pipeline pipeline() const { return *_pipeline; }
    vk::PipelineLayout layout() const { return _pipelineLayout; }

private:
    bool createGraphicsPipeline();

    vk::raii::Device& _dev;
    vk::PipelineLayout _pipelineLayout{};
    vk::RenderPass _renderPass{};
    vk::Format _swapchainFormat{};
    VertexLayout _layout{};
    std::shared_ptr<VKShader> _shader{};
    vk::raii::Pipeline _pipeline{nullptr};
    std::vector<vk::raii::Pipeline> _retired{};
    bool _created{false};
    bool _needsRecreate{false};

    bool _depthTest{false};
    bool _depthMask{true};
    CompareFunc _depthFunc{CompareFunc::Less};
    bool _cullEnable{false};
    CullFace _cullFace{CullFace::Back};
    bool _frontFaceCCW{true};
    bool _stencilTest{false};
    CompareFunc _stencilFunc{CompareFunc::Always};
    int _stencilRef{0};
    unsigned _stencilCompareMask{0xFF};
    StencilOp _stencilFail{StencilOp::Keep};
    StencilOp _stencilDepthFail{StencilOp::Keep};
    StencilOp _stencilPass{StencilOp::Keep};
    unsigned _stencilWriteMask{0xFF};
    bool _blend{false};
    BlendFactor _blendSrc{BlendFactor::SrcAlpha};
    BlendFactor _blendDst{BlendFactor::OneMinusSrcAlpha};
    PolygonMode _polygonMode{PolygonMode::Fill};
    bool _pointSizeEnable{false};
    bool _multisample{false};
    PrimitiveType _primitive{PrimitiveType::TriangleList};
};

} // namespace rhi