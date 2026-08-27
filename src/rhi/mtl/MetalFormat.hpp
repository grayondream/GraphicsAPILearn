#pragma once

#if defined(__APPLE__)

#include <Metal/Metal.h>
#include "rhi/core/Common.hpp"

namespace rhi::mtl {

inline MTLPixelFormat ToMTLPixelFormat(TextureFormat fmt) {
    switch (fmt) {
        case TextureFormat::RGB8:            return MTLPixelFormatRGBA8Unorm;
        case TextureFormat::RGBA8:           return MTLPixelFormatRGBA8Unorm;
        case TextureFormat::RGBA16F:         return MTLPixelFormatRGBA16Float;
        case TextureFormat::RGB16F:          return MTLPixelFormatRGBA16Float;
        case TextureFormat::RG16F:           return MTLPixelFormatRG16Float;
        case TextureFormat::R32F:            return MTLPixelFormatR32Float;
        case TextureFormat::RGBA32F:         return MTLPixelFormatRGBA32Float;
        case TextureFormat::Depth32F:        return MTLPixelFormatDepth32Float;
        case TextureFormat::Depth24Stencil8: return MTLPixelFormatDepth32Float_Stencil8;
    }
    return MTLPixelFormatRGBA8Unorm;
}

inline MTLVertexFormat ToMTLVertexFormat(VertexElement::Format fmt) {
    switch (fmt) {
        case VertexElement::Float2: return MTLVertexFormatFloat2;
        case VertexElement::Float3: return MTLVertexFormatFloat3;
        case VertexElement::Float4: return MTLVertexFormatFloat4;
        case VertexElement::Int4:   return MTLVertexFormatInt4;
    }
    return MTLVertexFormatFloat3;
}

inline MTLPrimitiveType ToMTLPrimitive(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::TriangleList:  return MTLPrimitiveTypeTriangle;
        case PrimitiveType::TriangleStrip: return MTLPrimitiveTypeTriangleStrip;
        case PrimitiveType::Lines:         return MTLPrimitiveTypeLine;
        case PrimitiveType::Points:        return MTLPrimitiveTypePoint;
    }
    return MTLPrimitiveTypeTriangle;
}

inline MTLCompareFunction ToMTLCompare(CompareFunc func) {
    switch (func) {
        case CompareFunc::Never:         return MTLCompareFunctionNever;
        case CompareFunc::Less:          return MTLCompareFunctionLess;
        case CompareFunc::Equal:         return MTLCompareFunctionEqual;
        case CompareFunc::LessEqual:     return MTLCompareFunctionLessEqual;
        case CompareFunc::Greater:       return MTLCompareFunctionGreater;
        case CompareFunc::NotEqual:      return MTLCompareFunctionNotEqual;
        case CompareFunc::GreaterEqual:  return MTLCompareFunctionGreaterEqual;
        case CompareFunc::Always:        return MTLCompareFunctionAlways;
    }
    return MTLCompareFunctionAlways;
}

inline MTLStencilOperation ToMTLStencilOp(StencilOp op) {
    switch (op) {
        case StencilOp::Keep:      return MTLStencilOperationKeep;
        case StencilOp::Zero:      return MTLStencilOperationZero;
        case StencilOp::Replace:   return MTLStencilOperationReplace;
        case StencilOp::Incr:      return MTLStencilOperationIncrementClamp;
        case StencilOp::Decr:      return MTLStencilOperationDecrementClamp;
        case StencilOp::IncrWrap:  return MTLStencilOperationIncrementWrap;
        case StencilOp::DecrWrap:  return MTLStencilOperationDecrementWrap;
    }
    return MTLStencilOperationKeep;
}

inline MTLBlendFactor ToMTLBlendFactor(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero:              return MTLBlendFactorZero;
        case BlendFactor::One:               return MTLBlendFactorOne;
        case BlendFactor::SrcAlpha:          return MTLBlendFactorSourceAlpha;
        case BlendFactor::OneMinusSrcAlpha:  return MTLBlendFactorOneMinusSourceAlpha;
        case BlendFactor::SrcColor:          return MTLBlendFactorSourceColor;
        case BlendFactor::OneMinusSrcColor:  return MTLBlendFactorOneMinusSourceColor;
    }
    return MTLBlendFactorOne;
}

inline MTLCullMode ToMTLCullMode(CullFace face, bool enable) {
    if (!enable) return MTLCullModeNone;
    switch (face) {
        case CullFace::Back:         return MTLCullModeBack;
        case CullFace::Front:        return MTLCullModeFront;
        case CullFace::FrontAndBack: return MTLCullModeNone;
    }
    return MTLCullModeNone;
}

inline MTLVertexDescriptor* BuildVertexDescriptor(const VertexLayout& layout) {
    MTLVertexDescriptor* desc = [[MTLVertexDescriptor alloc] init];
    for (size_t i = 0; i < layout.elements.size(); ++i) {
        const auto& elem = layout.elements[i];
        desc.attributes[i].format = ToMTLVertexFormat(elem.format);
        desc.attributes[i].offset = elem.offset;
        desc.attributes[i].bufferIndex = elem.binding;
        desc.layouts[elem.binding].stride = elem.stride;
        desc.layouts[elem.binding].stepFunction = (elem.inputRate == VertexInputRate::PerInstance)
            ? MTLVertexStepFunctionPerInstance
            : MTLVertexStepFunctionPerVertex;
    }
    return desc;
}

} // namespace rhi::mtl

#else // non-Apple fallback

namespace rhi::mtl {
// Metal format mapping not available on this platform
} // namespace rhi::mtl

#endif
