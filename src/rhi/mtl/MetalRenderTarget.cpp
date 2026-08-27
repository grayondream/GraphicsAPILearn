#if defined(__APPLE__)

#include "MetalRenderTarget.hpp"
#include "MetalTexture2D.hpp"
#include "MetalTexture3D.hpp"
#include "MetalFormat.hpp"
#include "base/Log.hpp"

namespace rhi::mtl {

MetalRenderTarget::MetalRenderTarget(void* device)
    : _device((__bridge id<MTLDevice>)device) {}

MetalRenderTarget::~MetalRenderTarget() { release(); }

bool MetalRenderTarget::create(int width, int height) {
    FramebufferDesc desc;
    desc.width = width;
    desc.height = height;
    desc.samples = 1;
    FramebufferAttachment color;
    color.type = AttachmentType::Color;
    color.format = TextureFormat::RGBA8;
    desc.attachments.push_back(color);
    FramebufferAttachment depth;
    depth.type = AttachmentType::DepthStencil;
    depth.format = TextureFormat::Depth24Stencil8;
    desc.attachments.push_back(depth);
    return create(desc);
}

bool MetalRenderTarget::create(const FramebufferDesc& desc) {
    release();
    if (!_device || desc.width <= 0 || desc.height <= 0) return false;

    _width = desc.width;
    _height = desc.height;
    _samples = desc.samples > 0 ? desc.samples : 1;
    _desc = desc;

    for (const auto& att : desc.attachments) {
        if (att.type == AttachmentType::Color) {
            auto tex = std::make_shared<MetalTexture2D>((void*)_device);
            TextureDesc td;
            td.format = att.format;
            td.minFilter = att.minFilter;
            td.magFilter = att.magFilter;
            td.wrapS = att.wrapS;
            td.wrapT = att.wrapT;
            if (_samples > 1) {
                td.multisample = true;
                td.samples = _samples;
            }
            td.generateMipmap = false;
            if (!tex->createEmpty(td, _width, _height)) {
                LOGE("[Metal] RT color attachment create failed");
                return false;
            }
            _colors.push_back(tex);
        } else if (!_depth) {
            auto tex = std::make_shared<MetalTexture2D>((void*)_device);
            TextureDesc td;
            td.format = att.format;
            td.minFilter = att.minFilter;
            td.magFilter = att.magFilter;
            td.wrapS = att.wrapS;
            td.wrapT = att.wrapT;
            if (_samples > 1) {
                td.multisample = true;
                td.samples = _samples;
            }
            td.generateMipmap = false;
            if (!tex->createEmpty(td, _width, _height)) {
                LOGE("[Metal] RT depth attachment create failed");
                return false;
            }
            _depth = tex;
        }
    }

    buildRenderPassDescriptor();
    _valid = true;
    return true;
}

bool MetalRenderTarget::attachCubeFace(ITexture3D* cube, int face, int mip) {
    auto* c3d = dynamic_cast<MetalTexture3D*>(cube);
    if (!c3d || !c3d->valid() || face < 0 || face > 5) {
        LOGW("[Metal] attachCubeFace: invalid cube/face");
        return false;
    }
    _cube = c3d;
    _face = face;
    _mip = mip;
    _cubeIsDepth = false;
    _cubeColorTexture = c3d->texture();
    buildRenderPassDescriptor();
    return true;
}

bool MetalRenderTarget::attachDepthCube(ITexture3D* cube, int mip) {
    auto* c3d = dynamic_cast<MetalTexture3D*>(cube);
    if (!c3d || !c3d->valid()) {
        LOGW("[Metal] attachDepthCube: not a valid cube");
        return false;
    }
    _cube = c3d;
    _face = -1;
    _mip = mip;
    _cubeIsDepth = true;
    _cubeDepthTexture = c3d->texture();
    return true;
}

bool MetalRenderTarget::bind() {
    return _valid;
}

bool MetalRenderTarget::unbind() {
    return true;
}

void* MetalRenderTarget::colorTexture() {
    id<MTLTexture> tex = activeColorTexture();
    return (__bridge void*)tex;
}

ITexture2D* MetalRenderTarget::colorTexture2D(int attachment) {
    if (attachment < 0) return nullptr;
    const size_t idx = static_cast<size_t>(attachment);
    if (idx < _colors.size()) return _colors[idx].get();
    return nullptr;
}

ITexture2D* MetalRenderTarget::depthTexture2D() {
    return _depth.get();
}

bool MetalRenderTarget::resolveTo(IRenderTarget& dst) {
    if (_samples <= 1) return false;

    auto& mtlDst = static_cast<MetalRenderTarget&>(dst);
    id<MTLTexture> srcTex = activeColorTexture();
    id<MTLTexture> dstTex = mtlDst.activeColorTexture();
    if (!srcTex || !dstTex) return false;

    id<MTLCommandQueue> queue = [_device newCommandQueue];
    if (!queue) return false;

    id<MTLCommandBuffer> cmd = [queue commandBuffer];
    if (!cmd) return false;

    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    if (!blit) return false;

    [blit copyFromTexture:srcTex
              sourceSlice:0
              sourceLevel:0
             sourceOrigin:MTLOriginMake(0, 0, 0)
               sourceSize:MTLSizeMake(_width, _height, 1)
                toTexture:dstTex
         destinationSlice:0
         destinationLevel:0
        destinationOrigin:MTLOriginMake(0, 0, 0)];

    [blit endEncoding];
    [cmd commit];
    return true;
}

void* MetalRenderTarget::handle() {
    return (__bridge void*)_rpd;
}

void MetalRenderTarget::release() {
    _colors.clear();
    _depth.reset();
    _rpd = nil;
    _cubeColorTexture = nil;
    _cubeDepthTexture = nil;
    _cube = nullptr;
    _face = -1;
    _mip = 0;
    _cubeIsDepth = false;
    _width = 0;
    _height = 0;
    _samples = 1;
    _valid = false;
}

id<MTLTexture> MetalRenderTarget::activeColorTexture() const {
    if (_cube && !_cubeIsDepth && _face >= 0 && _cubeColorTexture) {
        return _cubeColorTexture;
    }
    if (!_colors.empty()) return _colors[0]->texture();
    return nil;
}

id<MTLTexture> MetalRenderTarget::activeDepthTexture() const {
    if (_cube && _cubeIsDepth && _cubeDepthTexture) {
        return _cubeDepthTexture;
    }
    if (_depth) return _depth->texture();
    return nil;
}

void MetalRenderTarget::buildRenderPassDescriptor() {
    _rpd = [[MTLRenderPassDescriptor alloc] init];

    // Color attachment 0
    id<MTLTexture> colorTex = activeColorTexture();
    if (colorTex) {
        auto& colorAtt = _rpd.colorAttachments[0];
        colorAtt.texture = colorTex;
        colorAtt.loadAction = MTLLoadActionClear;
        colorAtt.clearColor = MTLClearColorMake(0, 0, 0, 1);

        if (_cube && !_cubeIsDepth && _face >= 0) {
            colorAtt.slice = _face;
            colorAtt.level = _mip;
            colorAtt.storeAction = MTLStoreActionStore;
        } else if (_samples > 1) {
            colorAtt.storeAction = MTLStoreActionMultisampleResolve;
            if (_colors.size() > 1) {
                colorAtt.resolveTexture = _colors[1]->texture();
            }
        } else {
            colorAtt.storeAction = MTLStoreActionStore;
        }
    }

    // Depth attachment
    id<MTLTexture> depthTex = activeDepthTexture();
    if (depthTex) {
        auto& depthAtt = _rpd.depthAttachment;
        depthAtt.texture = depthTex;
        depthAtt.loadAction = MTLLoadActionClear;
        depthAtt.clearDepth = 1.0;
        depthAtt.clearStencil = 0;

        if (_cube && _cubeIsDepth && _face >= 0) {
            depthAtt.slice = _face;
            depthAtt.level = _mip;
            depthAtt.storeAction = MTLStoreActionStore;
        } else {
            depthAtt.storeAction = _samples > 1 ? MTLStoreActionMultisampleResolve : MTLStoreActionStore;
        }
    }
}

} // namespace rhi::mtl

#endif
