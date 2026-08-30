#if defined(__APPLE__)

#import <Metal/Metal.h>
#include "MetalPipeline.hpp"
#include "MetalShader.hpp"
#include "MetalFormat.hpp"
#include "rhi/core/IBuffer.hpp"
#include "base/Log.hpp"

namespace rhi::mtl {

MetalPipeline::MetalPipeline(void* device)
    : _device(device) {}

MetalPipeline::~MetalPipeline() {
    _pipelineState = nil;
    _depthStencilState = nil;
    _vertexDescriptor = nil;
    _device = nullptr;
}

void MetalPipeline::use() {}

void* MetalPipeline::handle() {
    return _pipelineState ? (__bridge void*)_pipelineState : nullptr;
}

bool MetalPipeline::bindShader(const std::shared_ptr<MetalShader>& shader, const VertexLayout& layout) {
    _shader = shader;
    _layout = layout;
    _vertexDescriptor = BuildVertexDescriptor(layout);
    _pipelineState = nil;
    _depthStencilState = nil;
    _lastStateHash = 0;
    return true;
}

uint64_t MetalPipeline::stateHash() const {
    uint64_t h = 0;
    h = h * 31 + static_cast<uint64_t>(_depthTest);
    h = h * 31 + static_cast<uint64_t>(_depthFunc);
    h = h * 31 + static_cast<uint64_t>(_depthWrite);
    h = h * 31 + static_cast<uint64_t>(_stencilTest);
    h = h * 31 + static_cast<uint64_t>(_stencil.func);
    h = h * 31 + static_cast<uint64_t>(_stencil.reference);
    h = h * 31 + static_cast<uint64_t>(_stencil.mask);
    h = h * 31 + static_cast<uint64_t>(_stencil.opFail);
    h = h * 31 + static_cast<uint64_t>(_stencil.opDepthFail);
    h = h * 31 + static_cast<uint64_t>(_stencil.opDepthPass);
    h = h * 31 + static_cast<uint64_t>(_blend.enable);
    h = h * 31 + static_cast<uint64_t>(_blend.src);
    h = h * 31 + static_cast<uint64_t>(_blend.dst);
    h = h * 31 + static_cast<uint64_t>(_cullEnable);
    h = h * 31 + static_cast<uint64_t>(_cullFace);
    h = h * 31 + static_cast<uint64_t>(_frontCCW);
    h = h * 31 + static_cast<uint64_t>(_primitive);
    h = h * 31 + static_cast<uint64_t>(_multisample);
    return h;
}

void MetalPipeline::ensurePipeline(MTLPixelFormat colorFormat, MTLPixelFormat depthFormat) {
    if (!_shader || !_device) return;

    id<MTLDevice> device = (__bridge id<MTLDevice>)_device;
    uint64_t hash = stateHash();
    if (_pipelineState && hash == _lastStateHash
        && colorFormat == _lastColorFormat && depthFormat == _lastDepthFormat) {
        return;
    }

    @autoreleasepool {
        MTLRenderPipelineDescriptor* psoDesc = [[MTLRenderPipelineDescriptor alloc] init];

        id<MTLFunction> vertFn = _shader->vertexFunction();
        id<MTLFunction> fragFn = _shader->fragmentFunction();
        if (!vertFn || !fragFn) {
            LOGE("MetalPipeline: missing vertex or fragment function");
            return;
        }

        psoDesc.vertexFunction = vertFn;
        psoDesc.fragmentFunction = fragFn;
        psoDesc.vertexDescriptor = _vertexDescriptor;

        psoDesc.colorAttachments[0].pixelFormat = colorFormat;
        psoDesc.colorAttachments[0].blendingEnabled = _blend.enable;
        if (_blend.enable) {
            psoDesc.colorAttachments[0].sourceRGBBlendFactor = ToMTLBlendFactor(_blend.src);
            psoDesc.colorAttachments[0].destinationRGBBlendFactor = ToMTLBlendFactor(_blend.dst);
            psoDesc.colorAttachments[0].sourceAlphaBlendFactor = ToMTLBlendFactor(_blend.src);
            psoDesc.colorAttachments[0].destinationAlphaBlendFactor = ToMTLBlendFactor(_blend.dst);
        }

        if (depthFormat != MTLPixelFormatInvalid) {
            psoDesc.depthAttachmentPixelFormat = depthFormat;
        }
        if (depthFormat == MTLPixelFormatDepth32Float_Stencil8) {
            psoDesc.stencilAttachmentPixelFormat = depthFormat;
        }

        NSError* error = nil;
        _pipelineState = [device newRenderPipelineStateWithDescriptor:psoDesc error:&error];
        if (!_pipelineState || error) {
            NSString* errDesc = error ? [error localizedDescription] : @"unknown error";
            LOGE("MetalPipeline: PSO creation failed: {}", [errDesc UTF8String]);
            return;
        }

        MTLDepthStencilDescriptor* dsDesc = [[MTLDepthStencilDescriptor alloc] init];
        dsDesc.depthCompareFunction = _depthTest ? ToMTLCompare(_depthFunc) : MTLCompareFunctionAlways;
        dsDesc.depthWriteEnabled = _depthWrite;

        if (_stencilTest) {
            dsDesc.frontFaceStencil.stencilCompareFunction = ToMTLCompare(_stencil.func);
            dsDesc.frontFaceStencil.stencilFailureOperation = ToMTLStencilOp(_stencil.opFail);
            dsDesc.frontFaceStencil.depthFailureOperation = ToMTLStencilOp(_stencil.opDepthFail);
            dsDesc.frontFaceStencil.depthStencilPassOperation = ToMTLStencilOp(_stencil.opDepthPass);
            dsDesc.frontFaceStencil.readMask = _stencil.mask;
            dsDesc.frontFaceStencil.writeMask = _stencil.mask;
            dsDesc.backFaceStencil = dsDesc.frontFaceStencil;
        }

        _depthStencilState = [device newDepthStencilStateWithDescriptor:dsDesc];

        _lastStateHash = hash;
        _lastColorFormat = colorFormat;
        _lastDepthFormat = depthFormat;
    }
}

void MetalPipeline::applyRenderEncoder(void* encoder) {
    id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)encoder;
    if (!enc) return;

    [enc setRenderPipelineState:_pipelineState];

    if (_cullEnable) {
        [enc setCullMode:ToMTLCullMode(_cullFace, true)];
    } else {
        [enc setCullMode:MTLCullModeNone];
    }

    [enc setFrontFacingWinding:_frontCCW ? MTLWindingCounterClockwise : MTLWindingClockwise];

    if (_depthStencilState) {
        [enc setDepthStencilState:_depthStencilState];
    }
}

// ---- Uniform setters (Metal uses setVertexBytes/setFragmentBytes in renderer) ----

bool MetalPipeline::setUniform(const std::string&, bool) { return false; }
bool MetalPipeline::setUniform(const std::string&, int) { return false; }
bool MetalPipeline::setUniform(const std::string&, float) { return false; }
bool MetalPipeline::setUniform(const std::string&, const float*, int) { return false; }
bool MetalPipeline::setUniform(const std::string&, const float*, int, int) { return false; }
bool MetalPipeline::setUniformMatrix(const std::string&, const float*, int, int) { return false; }
void MetalPipeline::bindUniformBlock(uint32_t) {}

// ---- Render state setters (store values, PSO rebuilt lazily) ----

void MetalPipeline::setDepthTest(bool enable) { _depthTest = enable; _pipelineState = nil; }
void MetalPipeline::setDepthFunc(CompareFunc func) { _depthFunc = func; _pipelineState = nil; }
void MetalPipeline::setDepthMask(bool write) { _depthWrite = write; _pipelineState = nil; }
void MetalPipeline::setStencilTest(bool enable) { _stencilTest = enable; _pipelineState = nil; }
void MetalPipeline::setStencilFunc(CompareFunc func, int ref, unsigned mask) {
    _stencil.func = func;
    _stencil.reference = ref;
    _stencil.mask = mask;
    _pipelineState = nil;
}
void MetalPipeline::setStencilOp(StencilOp sfail, StencilOp dpfail, StencilOp dppass) {
    _stencil.opFail = sfail;
    _stencil.opDepthFail = dpfail;
    _stencil.opDepthPass = dppass;
    _pipelineState = nil;
}
void MetalPipeline::setStencilMask(unsigned mask) { _stencil.mask = mask; _pipelineState = nil; }
void MetalPipeline::setBlend(bool enable) { _blend.enable = enable; _pipelineState = nil; }
void MetalPipeline::setBlendFunc(BlendFactor src, BlendFactor dst) {
    _blend.src = src;
    _blend.dst = dst;
    _pipelineState = nil;
}
void MetalPipeline::setCullMode(bool enable, int face) { _cullEnable = enable; _cullFace = static_cast<CullFace>(face); }
void MetalPipeline::setCullFaceEnable(bool enable) { _cullEnable = enable; }
void MetalPipeline::setCullFace(CullFace face) { _cullFace = face; }
void MetalPipeline::setFrontFace(bool ccw) { _frontCCW = ccw; }
void MetalPipeline::setPolygonMode(PolygonMode mode) { _polygonMode = mode; }
void MetalPipeline::setPointSizeProgramEnable(bool enable) { _pointSizeEnable = enable; }
void MetalPipeline::setMultisample(bool enable) { _multisample = enable; _pipelineState = nil; }

void MetalPipeline::setPrimitiveType(PrimitiveType type) { _primitive = type; }
PrimitiveType MetalPipeline::primitiveType() const { return _primitive; }

void MetalPipeline::setVertexBuffer(const std::shared_ptr<IBuffer>&, uint32_t) {}
void MetalPipeline::setIndexBuffer(const std::shared_ptr<IBuffer>&) {}

} // namespace rhi::mtl

#endif
