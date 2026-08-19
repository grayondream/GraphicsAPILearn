#include "VKPipeline.hpp"
#include "base/Log.hpp"
#include <cstring>

namespace rhi {

static vk::Format ToVkFormat(VertexElement::Format format) {
    switch (format) {
        case VertexElement::Float2: return vk::Format::eR32G32Sfloat;
        case VertexElement::Float3: return vk::Format::eR32G32B32Sfloat;
        case VertexElement::Float4: return vk::Format::eR32G32B32A32Sfloat;
        case VertexElement::Int4:   return vk::Format::eR32G32B32A32Sint;
    }
    return vk::Format::eR32G32B32Sfloat;
}

static vk::PrimitiveTopology ToVkTopology(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::TriangleList:   return vk::PrimitiveTopology::eTriangleList;
        case PrimitiveType::TriangleStrip:  return vk::PrimitiveTopology::eTriangleStrip;
        case PrimitiveType::Lines:          return vk::PrimitiveTopology::eLineList;
        case PrimitiveType::Points:         return vk::PrimitiveTopology::ePointList;
    }
    return vk::PrimitiveTopology::eTriangleList;
}

static vk::CompareOp ToVkCompare(CompareFunc func) {
    switch (func) {
        case CompareFunc::Never:         return vk::CompareOp::eNever;
        case CompareFunc::Less:          return vk::CompareOp::eLess;
        case CompareFunc::Equal:         return vk::CompareOp::eEqual;
        case CompareFunc::LessEqual:     return vk::CompareOp::eLessOrEqual;
        case CompareFunc::Greater:       return vk::CompareOp::eGreater;
        case CompareFunc::NotEqual:      return vk::CompareOp::eNotEqual;
        case CompareFunc::GreaterEqual:  return vk::CompareOp::eGreaterOrEqual;
        case CompareFunc::Always:        return vk::CompareOp::eAlways;
    }
    return vk::CompareOp::eAlways;
}

static vk::StencilOp ToVkStencilOp(StencilOp op) {
    switch (op) {
        case StencilOp::Keep:       return vk::StencilOp::eKeep;
        case StencilOp::Zero:       return vk::StencilOp::eZero;
        case StencilOp::Replace:    return vk::StencilOp::eReplace;
        case StencilOp::Incr:       return vk::StencilOp::eIncrementAndClamp;
        case StencilOp::Decr:       return vk::StencilOp::eDecrementAndClamp;
        case StencilOp::IncrWrap:   return vk::StencilOp::eIncrementAndWrap;
        case StencilOp::DecrWrap:   return vk::StencilOp::eDecrementAndWrap;
    }
    return vk::StencilOp::eKeep;
}

static vk::BlendFactor ToVkBlendFactor(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero:                return vk::BlendFactor::eZero;
        case BlendFactor::One:                 return vk::BlendFactor::eOne;
        case BlendFactor::SrcAlpha:            return vk::BlendFactor::eSrcAlpha;
        case BlendFactor::OneMinusSrcAlpha:    return vk::BlendFactor::eOneMinusSrcAlpha;
        case BlendFactor::SrcColor:            return vk::BlendFactor::eSrcColor;
        case BlendFactor::OneMinusSrcColor:    return vk::BlendFactor::eOneMinusSrcColor;
    }
    return vk::BlendFactor::eOne;
}

static vk::PolygonMode ToVkPolygonMode(PolygonMode mode) {
    switch (mode) {
        case PolygonMode::Fill:   return vk::PolygonMode::eFill;
        case PolygonMode::Line:   return vk::PolygonMode::eLine;
        case PolygonMode::Point:  return vk::PolygonMode::ePoint;
    }
    return vk::PolygonMode::eFill;
}

static vk::CullModeFlags ToVkCullMode(bool enable, CullFace face) {
    if (!enable) return vk::CullModeFlagBits::eNone;
    switch (face) {
        case CullFace::Back:         return vk::CullModeFlagBits::eBack;
        case CullFace::Front:        return vk::CullModeFlagBits::eFront;
        case CullFace::FrontAndBack: return vk::CullModeFlagBits::eFrontAndBack;
    }
    return vk::CullModeFlagBits::eNone;
}

VKPipeline::VKPipeline(vk::raii::Device& device, vk::PipelineLayout layout, VertexLayout layoutIn,
                       std::shared_ptr<VKShader> shader)
    : _dev(device), _pipelineLayout(layout), _layout(std::move(layoutIn)), _shader(std::move(shader)) {}

void VKPipeline::use() {}

void* VKPipeline::handle() {
    // Pipelines are created per render pass and cached; there is no single
    // handle. Return the most recently built one for introspection use.
    if (!_cache.empty()) return reinterpret_cast<void*>(static_cast<VkPipeline>(*_cache.rbegin()->second));
    return nullptr;
}

void VKPipeline::setDepthTest(bool enable) { _depthTest = enable; }
void VKPipeline::setCullMode(bool enable, int face) { _cullEnable = enable; _cullFace = static_cast<CullFace>(face); }
void VKPipeline::setBlend(bool enable) { if (_blend != enable) { _blend = enable; _needsRecreate = true; } }
void VKPipeline::setDepthFunc(CompareFunc func) { _depthFunc = func; }
void VKPipeline::setDepthMask(bool write) { _depthMask = write; }
void VKPipeline::setStencilTest(bool enable) { _stencilTest = enable; }
void VKPipeline::setStencilFunc(CompareFunc func, int ref, unsigned mask) {
    _stencilFunc = func; _stencilRef = ref; _stencilCompareMask = mask;
}
void VKPipeline::setStencilOp(StencilOp sfail, StencilOp dpfail, StencilOp dppass) {
    _stencilFail = sfail; _stencilDepthFail = dpfail; _stencilPass = dppass;
}
void VKPipeline::setStencilMask(unsigned mask) { _stencilWriteMask = mask; }
void VKPipeline::setBlendFunc(BlendFactor src, BlendFactor dst) {
    if (_blendSrc != src || _blendDst != dst) { _blendSrc = src; _blendDst = dst; _needsRecreate = true; }
}
void VKPipeline::setCullFaceEnable(bool enable) { _cullEnable = enable; }
void VKPipeline::setCullFace(CullFace face) { _cullFace = face; }
void VKPipeline::setFrontFace(bool ccw) { _frontFaceCCW = ccw; }
void VKPipeline::setPolygonMode(PolygonMode mode) { if (_polygonMode != mode) { _polygonMode = mode; _needsRecreate = true; } }
void VKPipeline::setMultisample(bool enable) { if (_multisample != enable) { _multisample = enable; _needsRecreate = true; } }
void VKPipeline::setPrimitiveType(PrimitiveType type) {
    if (_primitive != type) { _primitive = type; _needsRecreate = true; }
}

vk::Pipeline VKPipeline::pipelineFor(vk::RenderPass rp, vk::SampleCountFlagBits samples, uint32_t colorCount) {
    if (_needsRecreate) {
        _cache.clear();
        _needsRecreate = false;
    }
    auto it = _cache.find(rp);
    if (it == _cache.end()) {
        vk::raii::Pipeline pipe{nullptr};
        if (!createGraphicsPipeline(rp, samples, colorCount, pipe)) {
            LOGE("VKPipeline: createGraphicsPipeline failed");
            return vk::Pipeline{};
        }
        it = _cache.emplace(rp, std::move(pipe)).first;
    }
    return *it->second;
}

bool VKPipeline::createGraphicsPipeline(vk::RenderPass rp, vk::SampleCountFlagBits samples, uint32_t colorCount, vk::raii::Pipeline& out) {
    const std::vector<vk::PipelineShaderStageCreateInfo> stages = _shader ? _shader->stageInfos() : std::vector<vk::PipelineShaderStageCreateInfo>{};
    if (stages.empty()) {
        LOGE("VKPipeline: no shader stages");
        return false;
    }

    std::vector<vk::VertexInputBindingDescription> bindings;
    std::vector<vk::VertexInputAttributeDescription> attributes;
    for (const auto& e : _layout.elements) {
        vk::VertexInputBindingDescription bid(e.binding, static_cast<uint32_t>(e.stride),
            e.inputRate == VertexInputRate::PerInstance ? vk::VertexInputRate::eInstance
                                                        : vk::VertexInputRate::eVertex);
        bool found = false;
        for (auto& b : bindings) if (b.binding == bid.binding) { b.stride = bid.stride; b.inputRate = bid.inputRate; found = true; break; }
        if (!found) bindings.push_back(bid);
        vk::VertexInputAttributeDescription aid(static_cast<uint32_t>(e.semantic), e.binding,
            ToVkFormat(e.format), static_cast<uint32_t>(e.offset));
        attributes.push_back(aid);
    }
    vk::PipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    vertexInput.pVertexBindingDescriptions = bindings.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = ToVkTopology(_primitive);
    inputAssembly.primitiveRestartEnable = vk::False;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode = ToVkPolygonMode(_polygonMode);
    rasterizer.cullMode = ToVkCullMode(_cullEnable, _cullFace);
    // 负高度 viewport 下 llvmpipe 不翻转 winding，直接用 GL 语义映射 front face
    //（CCW=true→eCounterClockwise, false→eClockwise），避免剔除判定颠倒成 no-op。
    rasterizer.frontFace = _frontFaceCCW ? vk::FrontFace::eCounterClockwise : vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisample{};
    multisample.rasterizationSamples = samples;
    multisample.sampleShadingEnable = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    // GL 语义：glDisable(GL_DEPTH_TEST) 与 glDepthMask(true) 相互独立——测试关、
    // 写开时仍会写深度。Vulkan 中 depthTestEnable=false 会使深度写入失效（即使
    // depthWriteEnable=true）。为对齐 GL，当需要写深度(_depthMask)时强制开启测试，
    // 测试原本关闭(_depthTest=false)时用 ALWAYS（恒通过≈测试关），从而仍写入深度，
    // 避免"先画的无测试物体被后画的物体覆盖"（如 Point 光源立方体跑到物体后面）。
    const bool depthTestEffective = _depthTest || _depthMask;
    depthStencil.depthTestEnable = depthTestEffective ? vk::True : vk::False;
    depthStencil.depthWriteEnable = _depthMask ? vk::True : vk::False;
    depthStencil.depthCompareOp = _depthTest ? ToVkCompare(_depthFunc) : vk::CompareOp::eAlways;
    depthStencil.depthBoundsTestEnable = vk::False;
    depthStencil.stencilTestEnable = _stencilTest ? vk::True : vk::False;
    depthStencil.front = vk::StencilOpState(ToVkStencilOp(_stencilFail), ToVkStencilOp(_stencilPass),
        ToVkStencilOp(_stencilDepthFail), ToVkCompare(_stencilFunc), _stencilCompareMask,
        _stencilWriteMask, static_cast<uint32_t>(_stencilRef));
    depthStencil.back = depthStencil.front;

    // One blend state per color attachment (MRT). Vulkan requires the pipeline's
    // attachment count to cover every color attachment of the subpass, otherwise
    // fragment outputs beyond attachmentCount-1 are dropped (e.g. Defer GBuffer
    // attachment 1/2 never written → black lighting pass).
    const uint32_t attachmentCount = std::max<uint32_t>(colorCount, 1u);
    std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments(attachmentCount);
    for (auto& blendAttach : blendAttachments) {
        blendAttach.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
            | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        blendAttach.blendEnable = _blend ? vk::True : vk::False;
        blendAttach.srcColorBlendFactor = ToVkBlendFactor(_blendSrc);
        blendAttach.dstColorBlendFactor = ToVkBlendFactor(_blendDst);
        blendAttach.colorBlendOp = vk::BlendOp::eAdd;
        blendAttach.srcAlphaBlendFactor = ToVkBlendFactor(_blendSrc);
        blendAttach.dstAlphaBlendFactor = ToVkBlendFactor(_blendDst);
        blendAttach.alphaBlendOp = vk::BlendOp::eAdd;
    }
    vk::PipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.logicOpEnable = vk::False;
    colorBlend.attachmentCount = attachmentCount;
    colorBlend.pAttachments = blendAttachments.data();

    const std::vector<vk::DynamicState> dyn = {
        vk::DynamicState::eViewport, vk::DynamicState::eScissor,
        vk::DynamicState::eDepthTestEnable, vk::DynamicState::eDepthWriteEnable, vk::DynamicState::eDepthCompareOp,
        vk::DynamicState::eCullMode, vk::DynamicState::eFrontFace,
        vk::DynamicState::eStencilTestEnable, vk::DynamicState::eStencilOp,
        vk::DynamicState::eStencilCompareMask, vk::DynamicState::eStencilWriteMask, vk::DynamicState::eStencilReference,
    };
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dyn.size());
    dynamicState.pDynamicStates = dyn.data();

    vk::GraphicsPipelineCreateInfo gpi{};
    gpi.stageCount = static_cast<uint32_t>(stages.size());
    gpi.pStages = stages.data();
    gpi.pVertexInputState = &vertexInput;
    gpi.pInputAssemblyState = &inputAssembly;
    gpi.pViewportState = &viewportState;
    gpi.pRasterizationState = &rasterizer;
    gpi.pMultisampleState = &multisample;
    gpi.pDepthStencilState = &depthStencil;
    gpi.pColorBlendState = &colorBlend;
    gpi.pDynamicState = &dynamicState;
    gpi.layout = _pipelineLayout;
    gpi.renderPass = rp;
    gpi.subpass = 0;

    auto pr = _dev.createGraphicsPipeline(nullptr, gpi);
    if (pr.result != vk::Result::eSuccess) {
        LOGE("VKPipeline: createGraphicsPipeline result {}", static_cast<int>(pr.result));
        return false;
    }
    out = std::move(pr.value);
    return true;
}

void VKPipeline::applyDynamicState(vk::raii::CommandBuffer& cmd) const {
    const bool depthTestEffective = _depthTest || _depthMask;
    cmd.setDepthTestEnable(depthTestEffective ? vk::True : vk::False);
    cmd.setDepthWriteEnable(_depthMask ? vk::True : vk::False);
    cmd.setDepthCompareOp(_depthTest ? ToVkCompare(_depthFunc) : vk::CompareOp::eAlways);
    cmd.setCullMode(ToVkCullMode(_cullEnable, _cullFace));
    // 与 pipeline 创建一致（llvmpipe 负高度 viewport 不翻转 winding，直接用 GL 语义）
    cmd.setFrontFace(_frontFaceCCW ? vk::FrontFace::eCounterClockwise : vk::FrontFace::eClockwise);
    cmd.setStencilTestEnable(_stencilTest ? vk::True : vk::False);
    cmd.setStencilOp(vk::StencilFaceFlagBits::eFrontAndBack, ToVkStencilOp(_stencilFail),
        ToVkStencilOp(_stencilPass), ToVkStencilOp(_stencilDepthFail), ToVkCompare(_stencilFunc));
    cmd.setStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, _stencilCompareMask);
    cmd.setStencilWriteMask(vk::StencilFaceFlagBits::eFrontAndBack, _stencilWriteMask);
    cmd.setStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, static_cast<uint32_t>(_stencilRef));
}

} // namespace rhi