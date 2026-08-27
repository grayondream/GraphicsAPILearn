#if defined(__APPLE__)

#include "MetalBackend.hpp"
#include "MetalShader.hpp"
#include "MetalPipeline.hpp"
#include "MetalBuffer.hpp"
#include "MetalTexture2D.hpp"
#include "MetalTexture3D.hpp"
#include "MetalRenderTarget.hpp"
#include "MetalSwapchain.hpp"
#include "MetalSurface.hpp"
#include "MetalFormat.hpp"
#include "rhi/core/ISurface.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/ITexture3D.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ISwapchain.hpp"
#include "base/Log.hpp"

#import <QuartzCore/CAMetalLayer.h>

#include <array>
#include <map>
#include <cstring>

namespace rhi::mtl {

namespace {

static MTLPrimitiveType ToMTLPrimitive(PrimitiveType t) {
    switch (t) {
        case PrimitiveType::TriangleList:  return MTLPrimitiveTypeTriangle;
        case PrimitiveType::TriangleStrip: return MTLPrimitiveTypeTriangleStrip;
        case PrimitiveType::Lines:         return MTLPrimitiveTypeLine;
        case PrimitiveType::Points:        return MTLPrimitiveTypePoint;
    }
    return MTLPrimitiveTypeTriangle;
}

} // namespace

class MetalRenderer : public IRenderer {
public:
    ~MetalRenderer() override { shutdown(); }

    bool init(const std::shared_ptr<ISurface>& surface) override {
        if (!surface) return false;
        _surface = surface;

        auto* mtlSurface = static_cast<MetalSurface*>(surface.get());
        _layer = mtlSurface->layer();
        if (!_layer) {
            LOGE("[Metal] init: CAMetalLayer is null");
            return false;
        }

        _device = MTLCreateSystemDefaultDevice();
        if (!_device) {
            LOGE("[Metal] init: MTLCreateSystemDefaultDevice failed");
            return false;
        }

        _queue = [_device newCommandQueue];
        if (!_queue) {
            LOGE("[Metal] init: newCommandQueue failed");
            return false;
        }

        _swapchain = std::make_shared<MetalSwapchain>();
        _swapchain->init(_layer, mtlSurface->width(), mtlSurface->height());

        _renderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];

        LOGI("Metal Device: {}", [[_device name] UTF8String]);
        return true;
    }

    void shutdown() override {
        _commandBuffer = nil;
        _renderEncoder = nil;
        _blitEncoder = nil;
        _currentDrawable = nil;
        _swapchain.reset();
        _queue = nil;
        _device = nil;
        _renderPassDescriptor = nil;
        _surface.reset();
    }

    // ---- Resource factories ----
    std::shared_ptr<IShader> createShader() override {
        return std::make_shared<MetalShader>((void*)_device);
    }
    std::shared_ptr<IPipeline> createPipeline(const VertexLayout& layout, const std::shared_ptr<IShader>& shader) override {
        auto mtlShader = std::dynamic_pointer_cast<MetalShader>(shader);
        if (!_device || !mtlShader || !mtlShader->valid()) {
            LOGE("[Metal] createPipeline: device/shader not ready");
            return nullptr;
        }
        auto p = std::make_shared<MetalPipeline>((void*)_device);
        p->bindShader(mtlShader.get(), layout);
        return p;
    }
    std::shared_ptr<IBuffer> createBuffer() override {
        if (!_device) return nullptr;
        return std::make_shared<MetalBuffer>((void*)_device);
    }
    std::shared_ptr<IBuffer> createUniformBuffer() override {
        if (!_device) return nullptr;
        return std::make_shared<MetalBuffer>((void*)_device);
    }
    std::shared_ptr<ITexture2D> createTexture2D() override {
        if (!_device) return nullptr;
        return std::make_shared<MetalTexture2D>((void*)_device);
    }
    std::shared_ptr<ITexture3D> createTexture3D() override {
        if (!_device) return nullptr;
        return std::make_shared<MetalTexture3D>((void*)_device);
    }
    std::shared_ptr<IRenderTarget> createRenderTarget() override {
        if (!_device) return nullptr;
        return std::make_shared<MetalRenderTarget>((void*)_device);
    }
    std::shared_ptr<ISwapchain> getSwapchain() override { return _swapchain; }

    // ---- Frame control ----
    void beginFrame() override {
        if (!_swapchain || !_queue) return;

        _currentDrawable = _swapchain->currentDrawable();
        if (!_currentDrawable) return;

        _commandBuffer = [_queue commandBuffer];
        if (!_commandBuffer) return;

        if (!_renderPassDescriptor) {
            _renderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];
        }

        buildRenderPassDescriptor();

        _renderEncoder = [_commandBuffer renderCommandEncoderWithDescriptor:_renderPassDescriptor];
        _encodingActive = true;
    }

    void endFrame() override {
        if (_renderEncoder && _encodingActive) {
            [_renderEncoder endEncoding];
            _renderEncoder = nil;
            _encodingActive = false;
        }
        if (_commandBuffer) {
            [_commandBuffer commit];
            _commandBuffer = nil;
        }
    }

    bool present() override {
        if (!_currentDrawable || !_swapchain) return false;
        [_currentDrawable presentAfterMinimumDuration:0.0];
        _currentDrawable = nil;
        return true;
    }

    // ---- State ----
    void clearColor(float r, float g, float b, float a) override {
        _clearColor[0] = r; _clearColor[1] = g; _clearColor[2] = b; _clearColor[3] = a;
    }

    void setViewport(const Viewport& vp) override {
        _viewport = vp;
    }

    void setPipeline(const std::shared_ptr<IPipeline>& pipeline) override {
        _pipeline = pipeline;
    }

    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer) override {
        setVertexBuffer(buffer, 0);
    }

    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer, uint32_t binding) override {
        if (binding < _vertexBuffers.size()) {
            _vertexBuffers[binding] = buffer;
        }
    }

    void setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) override {
        _indexBuffer = buffer;
    }

    void setRenderTarget(const std::shared_ptr<IRenderTarget>& target) override {
        _renderTarget = target;
    }

    void bindTexture(const std::shared_ptr<ITexture2D>& texture, unsigned int unit) override {
        if (texture && _renderEncoder) {
            auto* mtlTex = dynamic_cast<MetalTexture2D*>(texture.get());
            if (mtlTex && mtlTex->valid()) {
                [_renderEncoder setFragmentTexture:mtlTex->texture() atIndex:unit];
                if (mtlTex->sampler()) {
                    [_renderEncoder setFragmentSamplerState:mtlTex->sampler() atIndex:unit];
                }
            }
        }
    }

    void bindTexture(const std::shared_ptr<ITexture3D>& texture, unsigned int unit) override {
        if (texture && _renderEncoder) {
            auto* mtlTex = dynamic_cast<MetalTexture3D*>(texture.get());
            if (mtlTex && mtlTex->valid()) {
                [_renderEncoder setFragmentTexture:mtlTex->texture() atIndex:unit];
                if (mtlTex->sampler()) {
                    [_renderEncoder setFragmentSamplerState:mtlTex->sampler() atIndex:unit];
                }
            }
        }
    }

    void bindTexture(rhi::ITexture2D* texture, unsigned int unit) override {
        if (texture && _renderEncoder) {
            auto* mtlTex = dynamic_cast<MetalTexture2D*>(texture);
            if (mtlTex && mtlTex->valid()) {
                [_renderEncoder setFragmentTexture:mtlTex->texture() atIndex:unit];
                if (mtlTex->sampler()) {
                    [_renderEncoder setFragmentSamplerState:mtlTex->sampler() atIndex:unit];
                }
            }
        }
    }

    // ---- Draw ----
    void draw(uint32_t vertexCount, uint32_t firstVertex) override {
        if (!prepareDraw()) return;
        [_renderEncoder drawPrimitives:ToMTLPrimitive(_pipeline ? _pipeline->primitiveType() : PrimitiveType::TriangleList)
                          vertexStart:firstVertex
                          vertexCount:vertexCount];
    }

    void drawIndexed(uint32_t indexCount, uint32_t indexOffset, uint32_t vertexOffset) override {
        if (!prepareDraw()) return;
        auto* idxBuf = dynamic_cast<MetalBuffer*>(_indexBuffer.get());
        if (!idxBuf || !idxBuf->handle()) return;

        [_renderEncoder drawIndexedPrimitives:ToMTLPrimitive(_pipeline ? _pipeline->primitiveType() : PrimitiveType::TriangleList)
                                   indexCount:indexCount
                                    indexType:MTLIndexTypeUInt32
                                  indexBuffer:(__bridge id<MTLBuffer>)idxBuf->handle()
                            indexBufferOffset:indexOffset * sizeof(uint32_t)
                                instanceCount:1
                                   baseVertex:vertexOffset
                               baseInstance:0];
    }

    void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount,
                              uint32_t indexOffset, uint32_t vertexOffset) override {
        if (!prepareDraw()) return;
        auto* idxBuf = dynamic_cast<MetalBuffer*>(_indexBuffer.get());
        if (!idxBuf || !idxBuf->handle()) return;

        [_renderEncoder drawIndexedPrimitives:ToMTLPrimitive(_pipeline ? _pipeline->primitiveType() : PrimitiveType::TriangleList)
                                   indexCount:indexCount
                                    indexType:MTLIndexTypeUInt32
                                  indexBuffer:(__bridge id<MTLBuffer>)idxBuf->handle()
                            indexBufferOffset:indexOffset * sizeof(uint32_t)
                                instanceCount:instanceCount
                                   baseVertex:vertexOffset
                               baseInstance:0];
    }

    void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) override {
        if (!prepareDraw()) return;
        [_renderEncoder drawPrimitives:ToMTLPrimitive(_pipeline ? _pipeline->primitiveType() : PrimitiveType::TriangleList)
                          vertexStart:firstVertex
                          vertexCount:vertexCount
                        instanceCount:instanceCount];
    }

    // ---- Blit ----
    void blitFramebuffer(const std::shared_ptr<IRenderTarget>& src,
                         const std::shared_ptr<IRenderTarget>& dst,
                         BlitMask mask) override {
        if (!src || !_queue) return;

        auto* srcRT = dynamic_cast<MetalRenderTarget*>(src.get());
        if (!srcRT) return;

        bool doColor = (static_cast<uint8_t>(mask) & static_cast<uint8_t>(BlitMask::Color)) != 0;
        bool doDepth = (static_cast<uint8_t>(mask) & static_cast<uint8_t>(BlitMask::Depth)) != 0;

        if (doColor && dst) {
            auto* dstRT = dynamic_cast<MetalRenderTarget*>(dst.get());
            id<MTLTexture> srcColor = srcRT->activeColorTexture();
            id<MTLTexture> dstColor = dstRT ? dstRT->activeColorTexture() : nil;
            if (dstRT && srcColor && dstColor) {
                id<MTLCommandBuffer> cmd = [_queue commandBuffer];
                id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];

                MTLSize srcSize = MTLSizeMake(srcColor.width, srcColor.height, 1);
                [blit copyFromTexture:srcColor
                          sourceSlice:0
                          sourceLevel:0
                         sourceOrigin:MTLOriginMake(0, 0, 0)
                           sourceSize:srcSize
                            toTexture:dstColor
                     destinationSlice:0
                     destinationLevel:0
                    destinationOrigin:MTLOriginMake(0, 0, 0)];

                [blit endEncoding];
                [cmd commit];
            }
        }

        if (doDepth) {
            id<MTLTexture> srcDepth = srcRT->activeDepthTexture();
            id<MTLTexture> dstDepth = nil;
            if (dst) {
                auto* dstRT = dynamic_cast<MetalRenderTarget*>(dst.get());
                if (dstRT) dstDepth = dstRT->activeDepthTexture();
            }
            if (srcDepth && dstDepth) {
                id<MTLCommandBuffer> cmd = [_queue commandBuffer];
                id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];

                MTLSize srcSize = MTLSizeMake(srcDepth.width, srcDepth.height, 1);
                [blit copyFromTexture:srcDepth
                          sourceSlice:0
                          sourceLevel:0
                         sourceOrigin:MTLOriginMake(0, 0, 0)
                           sourceSize:srcSize
                            toTexture:dstDepth
                     destinationSlice:0
                     destinationLevel:0
                    destinationOrigin:MTLOriginMake(0, 0, 0)];

                [blit endEncoding];
                [cmd commit];
            }
        }
    }

    // ---- Capabilities ----
    BackendCapabilities backendCapabilities() override {
        BackendCapabilities caps{};
        caps.maxUniformBlockSize = 64 * 1024;
        if (_device) {
            if ([_device supportsFamily:MTLGPUFamilyApple3]) {
                caps.maxSamples = 8;
            } else if ([_device supportsFamily:MTLGPUFamilyApple1]) {
                caps.maxSamples = 4;
            } else {
                caps.maxSamples = 1;
            }
        }
        return caps;
    }

    // ---- State management ----
    void resetRenderState() override {
        _pipeline = nullptr;
        _indexBuffer = nullptr;
        _vertexBuffers = {};
        _renderTarget = nullptr;
        _clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        _viewport = {};
    }

    void waitIdle() override {
        if (_commandBuffer) {
            [_commandBuffer waitUntilCompleted];
        }
    }

    void flush() override {
        endFrame();
    }

    // ---- ImGui ----
    using IRenderer::imguiInitInfo;
    bool imguiInitInfo(MetalImGuiInitInfo& out) {
        out.device = _device;
        out.queue = _queue;
        return _device && _queue;
    }

    void renderImGuiDrawData(void* drawData) override {
        (void)drawData;
    }

private:
    void buildRenderPassDescriptor() {
        if (!_renderPassDescriptor) return;

        _renderPassDescriptor.colorAttachments[0].texture = _currentDrawable.texture;
        _renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        _renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(
            _clearColor[0], _clearColor[1], _clearColor[2], _clearColor[3]);
        _renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

        if (_renderTarget) {
            auto* rt = dynamic_cast<MetalRenderTarget*>(_renderTarget.get());
            if (rt && rt->valid()) {
                id<MTLTexture> depthTex = rt->activeDepthTexture();
                if (depthTex) {
                    _renderPassDescriptor.depthAttachment.texture = depthTex;
                    _renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
                    _renderPassDescriptor.depthAttachment.clearDepth = 1.0;
                    _renderPassDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
                }
            }
        }
    }

    bool prepareDraw() {
        if (!_renderEncoder || !_pipeline) return false;

        auto* mtlPipeline = dynamic_cast<MetalPipeline*>(_pipeline.get());
        if (!mtlPipeline) return false;

        MTLPixelFormat colorFmt = _currentDrawable ? _currentDrawable.texture.pixelFormat : MTLPixelFormatBGRA8Unorm;
        MTLPixelFormat depthFmt = MTLPixelFormatDepth32Float;
        mtlPipeline->ensurePipeline(colorFmt, depthFmt);
        mtlPipeline->applyRenderEncoder(_renderEncoder);

        if (_viewport.width > 0 && _viewport.height > 0) {
            MTLViewport vp;
            vp.originX = _viewport.x;
            vp.originY = _viewport.y;
            vp.width = _viewport.width;
            vp.height = _viewport.height;
            vp.znear = 0.0;
            vp.zfar = 1.0;
            [_renderEncoder setViewport:vp];
        }

        for (uint32_t i = 0; i < _vertexBuffers.size(); ++i) {
            auto* buf = dynamic_cast<MetalBuffer*>(_vertexBuffers[i].get());
            if (buf && buf->handle()) {
                [_renderEncoder setVertexBuffer:(__bridge id<MTLBuffer>)buf->handle()
                                         offset:0
                                        atIndex:i];
            }
        }

        return true;
    }

    std::shared_ptr<ISurface> _surface{};
    CAMetalLayer* _layer{nullptr};
    id<MTLDevice> _device{nil};
    id<MTLCommandQueue> _queue{nil};
    std::shared_ptr<ISwapchain> _swapchain{};

    id<CAMetalDrawable> _currentDrawable{nil};
    id<MTLCommandBuffer> _commandBuffer{nil};
    id<MTLRenderCommandEncoder> _renderEncoder{nil};
    id<MTLRenderPassDescriptor> _renderPassDescriptor{nil};

    std::shared_ptr<IPipeline> _pipeline{};
    std::array<std::shared_ptr<IBuffer>, 8> _vertexBuffers{};
    std::shared_ptr<IBuffer> _indexBuffer{};
    std::shared_ptr<IRenderTarget> _renderTarget{};

    Viewport _viewport{};
    std::array<float, 4> _clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    bool _encodingActive{false};
};

std::shared_ptr<IRenderer> createMetalRenderer() {
    return std::make_shared<MetalRenderer>();
}

bool GetMetalImGuiInitInfo(const std::shared_ptr<IRenderer>& renderer, MetalImGuiInitInfo& out) {
    if (!renderer) return false;
    auto* mtlRenderer = static_cast<MetalRenderer*>(renderer.get());
    return mtlRenderer->imguiInitInfo(out);
}

} // namespace rhi::mtl

#else // non-Apple fallback

#include "MetalBackend.hpp"

namespace rhi::mtl {

} // namespace rhi::mtl

#endif
