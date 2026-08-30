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
#import <Metal/Metal.h>

#include <array>
#include <map>
#include <cstring>
#include <cstdio>

namespace rhi::mtl {

class MetalRenderer : public IRenderer {
public:
    ~MetalRenderer() override { shutdown(); }

    bool init(const std::shared_ptr<ISurface>& surface) override {
        if (!surface) return false;
        _surface = surface;

        auto* mtlSurface = static_cast<MetalSurface*>(surface.get());
        _layer = (__bridge CAMetalLayer*)mtlSurface->layer();
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

        _layer.device = _device;

        _mtlSwapchain = std::make_shared<MetalSwapchain>();
        _mtlSwapchain->init(_layer, mtlSurface->width(), mtlSurface->height());
        _swapchain = _mtlSwapchain;

        _renderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];

        LOGI("Metal Device: {}", [[_device name] UTF8String]);
        return true;
    }

    void shutdown() override {
        _commandBuffer = nil;
        _renderEncoder = nil;
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
        p->bindShader(mtlShader, layout);
        return p;
    }
    std::shared_ptr<IBuffer> createBuffer() override {
        if (!_device) return nullptr;
        return std::make_shared<MetalBuffer>((void*)_device);
    }
    std::shared_ptr<IBuffer> createUniformBuffer() override {
        if (!_device) return nullptr;
        auto buf = std::make_shared<MetalBuffer>((void*)_device);
        std::weak_ptr<MetalBuffer> weakBuf = buf;
        buf->setBindCallback([this, weakBuf](MetalBuffer*, uint32_t) {
            auto b = weakBuf.lock();
            if (!b) return;
            bool found = false;
            for (const auto& e : _uniformBuffers) {
                if (e.get() == b.get()) { found = true; break; }
            }
            if (!found) _uniformBuffers.push_back(b);
        });
        return buf;
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

        _currentDrawable = _mtlSwapchain->currentDrawable();
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
        if (getenv("METAL_READBACK")) {
            static int frameCount = 0;
            ++frameCount;
            @autoreleasepool {
                @try {
                    id<MTLTexture> src = _currentDrawable.texture;
                    if (src) {
                        MTLTextureDescriptor* td =
                            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:src.pixelFormat
                                                                               width:src.width
                                                                              height:src.height
                                                                           mipmapped:NO];
                        td.storageMode = MTLStorageModeShared;
                        id<MTLTexture> dst = [(__bridge id<MTLDevice>)_device newTextureWithDescriptor:td];
                        id<MTLCommandBuffer> cb = [_queue commandBuffer];
                        id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
                        [blit copyFromTexture:src toTexture:dst];
                        [blit endEncoding];
                        [cb commit];
                        [cb waitUntilCompleted];
                        std::vector<uint8_t> px((size_t)src.width * (size_t)src.height * 4);
                        [dst getBytes:px.data()
                            bytesPerRow:src.width * 4
                             fromRegion:MTLRegionMake2D(0, 0, src.width, src.height)
                            mipmapLevel:0];

                        auto pixelAt = [&](int x, int y) -> std::string {
                            if (x < 0 || x >= (int)src.width || y < 0 || y >= (int)src.height) return "OOB";
                            size_t idx = ((size_t)y * src.width + x) * 4;
                            return "(" + std::to_string(px[idx]) + "," + std::to_string(px[idx+1]) + "," + std::to_string(px[idx+2]) + "," + std::to_string(px[idx+3]) + ")";
                        };

                        int w = (int)src.width, h = (int)src.height;
                        LOGI("READBACK frame={} w={} h={}", frameCount, w, h);
                        LOGI("  corners: TL={} TR={} BL={} BR={}",
                             pixelAt(0, 0), pixelAt(w-1, 0), pixelAt(0, h-1), pixelAt(w-1, h-1));
                        LOGI("  center: {}  midTop={} midBot={} midLeft={} midRight={}",
                             pixelAt(w/2, h/2), pixelAt(w/2, 0), pixelAt(w/2, h-1),
                             pixelAt(0, h/2), pixelAt(w-1, h/2));
                        LOGI("  viewport=({},{},{},{})", _viewport.x, _viewport.y, _viewport.width, _viewport.height);

                        long nonblack = 0, bright = 0, maxlum = 0;
                        for (long i = 0; i < (long)w * (long)h; ++i) {
                            int l = px[i * 4] + px[i * 4 + 1] + px[i * 4 + 2];
                            if (l > 12) ++nonblack;
                            if (l > 200) ++bright;
                            if (l > maxlum) maxlum = l;
                        }
                        LOGI("  stats: nonblack={} bright={} maxlum={}", nonblack, bright, (int)maxlum);

                        if (frameCount <= 3) {
                            char path[256];
                            snprintf(path, sizeof(path), "/tmp/metal_frame%d.bmp", frameCount);
                            FILE* f = fopen(path, "wb");
                            if (f) {
                                int rowBytes = w * 4;
                                int paddedRow = (rowBytes + 3) & ~3;
                                int dataSize = paddedRow * h;
                                int fileSize = 14 + 40 + dataSize;
                                // BMP file header (14 bytes)
                                uint8_t fileHeader[14] = {};
                                fileHeader[0] = 'B'; fileHeader[1] = 'M';
                                fileHeader[2] = fileSize & 0xFF;
                                fileHeader[3] = (fileSize >> 8) & 0xFF;
                                fileHeader[4] = (fileSize >> 16) & 0xFF;
                                fileHeader[5] = (fileSize >> 24) & 0xFF;
                                fileHeader[10] = 54; // pixel data offset
                                // DIB header (40 bytes)
                                uint8_t dibHeader[40] = {};
                                dibHeader[0] = 40; // header size
                                dibHeader[4] = w & 0xFF;
                                dibHeader[5] = (w >> 8) & 0xFF;
                                dibHeader[6] = (w >> 16) & 0xFF;
                                dibHeader[7] = (w >> 24) & 0xFF;
                                dibHeader[8] = h & 0xFF;
                                dibHeader[9] = (h >> 8) & 0xFF;
                                dibHeader[10] = (h >> 16) & 0xFF;
                                dibHeader[11] = (h >> 24) & 0xFF;
                                dibHeader[12] = 1; // planes
                                dibHeader[14] = 32; // bits per pixel
                                dibHeader[20] = dataSize & 0xFF;
                                dibHeader[21] = (dataSize >> 8) & 0xFF;
                                dibHeader[22] = (dataSize >> 16) & 0xFF;
                                dibHeader[23] = (dataSize >> 24) & 0xFF;
                                fwrite(fileHeader, 1, 14, f);
                                fwrite(dibHeader, 1, 40, f);
                                // BMP stores bottom-up, BGRA
                                std::vector<uint8_t> row(paddedRow, 0);
                                for (int y = h - 1; y >= 0; --y) {
                                    for (int x = 0; x < w; ++x) {
                                        size_t si = ((size_t)y * w + x) * 4;
                                        row[x * 4 + 0] = px[si + 2]; // B
                                        row[x * 4 + 1] = px[si + 1]; // G
                                        row[x * 4 + 2] = px[si + 0]; // R
                                        row[x * 4 + 3] = px[si + 3]; // A
                                    }
                                    fwrite(row.data(), 1, paddedRow, f);
                                }
                                fclose(f);
                                LOGI("  saved: {}", path);
                            }
                        }
                    }
                } @catch (NSException* e) {
                    LOGE("READBACK exception: {}", [[e reason] UTF8String]);
                }
            }
        }
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
        recreateEncoder();
    }

    void bindTexture(const std::shared_ptr<ITexture2D>& texture, unsigned int unit) override {
        if (texture && _renderEncoder) {
            auto* mtlTex = dynamic_cast<MetalTexture2D*>(texture.get());
            if (mtlTex && mtlTex->valid()) {
                [_renderEncoder setFragmentTexture:(__bridge id<MTLTexture>)mtlTex->texture() atIndex:unit];
                if (mtlTex->sampler()) {
                    [_renderEncoder setFragmentSamplerState:(__bridge id<MTLSamplerState>)mtlTex->sampler() atIndex:unit];
                }
            }
        }
    }

    void bindTexture(const std::shared_ptr<ITexture3D>& texture, unsigned int unit) override {
        if (texture && _renderEncoder) {
            auto* mtlTex = dynamic_cast<MetalTexture3D*>(texture.get());
            if (mtlTex && mtlTex->valid()) {
                [_renderEncoder setFragmentTexture:(__bridge id<MTLTexture>)mtlTex->texture() atIndex:unit];
                if (mtlTex->sampler()) {
                    [_renderEncoder setFragmentSamplerState:(__bridge id<MTLSamplerState>)mtlTex->sampler() atIndex:unit];
                }
            }
        }
    }

    void bindTexture(rhi::ITexture2D* texture, unsigned int unit) override {
        if (texture && _renderEncoder) {
            auto* mtlTex = dynamic_cast<MetalTexture2D*>(texture);
            if (mtlTex && mtlTex->valid()) {
                [_renderEncoder setFragmentTexture:(__bridge id<MTLTexture>)mtlTex->texture() atIndex:unit];
                if (mtlTex->sampler()) {
                    [_renderEncoder setFragmentSamplerState:(__bridge id<MTLSamplerState>)mtlTex->sampler() atIndex:unit];
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
        if (!idxBuf || !idxBuf->handle()) {
            LOGE("[Metal] drawIndexed: index buffer is null! indexCount={}", indexCount);
            return;
        }

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
        _uniformBuffers.clear();
        _renderTarget = nullptr;
        _clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
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
        out.commandQueue = _queue;
        out.renderPassDescriptor = _renderPassDescriptor;
        out.commandBuffer = _commandBuffer;
        out.renderEncoder = _renderEncoder;
        return _device && _queue;
    }

    void renderImGuiDrawData(void* drawData) override {
        (void)drawData;
    }

private:
    void buildRenderPassDescriptor() {
        if (!_renderPassDescriptor) return;

        id<MTLTexture> colorTex = _currentDrawable.texture;
        id<MTLTexture> depthTex = nil;

        if (_renderTarget) {
            auto* rt = dynamic_cast<MetalRenderTarget*>(_renderTarget.get());
            if (rt && rt->valid()) {
                id<MTLTexture> rtColor = rt->activeColorTexture();
                if (rtColor) colorTex = rtColor;
                depthTex = rt->activeDepthTexture();
            }
        } else if (_mtlSwapchain) {
            depthTex = _mtlSwapchain->depthTexture();
        }

        _renderPassDescriptor.colorAttachments[0].texture = colorTex;
        _renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        _renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(
            _clearColor[0], _clearColor[1], _clearColor[2], _clearColor[3]);
        _renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

        // 默认无深度附件（与 PSO 的 MTLPixelFormatInvalid 对应）。主交换链通道
        // 需要深度缓冲时，挂上交换链自带的 depth 纹理；离屏 RT 则使用其自身 depth。
        // 注意：MTLRenderPassAttachmentDescriptor 没有 pixelFormat 属性，格式由 texture 推断，
        // 因此 PSO 的 depthAttachmentPixelFormat 必须与 depthTex.pixelFormat 一致。
        if (depthTex) {
            _renderPassDescriptor.depthAttachment.texture = depthTex;
            _renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
            _renderPassDescriptor.depthAttachment.clearDepth = 1.0;
            _renderPassDescriptor.depthAttachment.storeAction =
                _renderTarget ? MTLStoreActionStore : MTLStoreActionDontCare;
        } else {
            _renderPassDescriptor.depthAttachment.texture = nil;
        }
    }

    // 切换渲染目标（离屏 RT <-> 交换链）时必须重建渲染编码器，
    // 否则编码器仍使用 beginFrame 时构建的旧通道描述符（颜色附件指向交换链）。
    void recreateEncoder() {
        if (_renderEncoder && _encodingActive) {
            [_renderEncoder endEncoding];
            _renderEncoder = nil;
            _encodingActive = false;
        }
        if (!_commandBuffer) {
            _commandBuffer = [_queue commandBuffer];
            if (!_commandBuffer) return;
        }
        buildRenderPassDescriptor();
        _renderEncoder = [_commandBuffer renderCommandEncoderWithDescriptor:_renderPassDescriptor];
        _encodingActive = true;
    }

    bool prepareDraw() {
        if (!_renderEncoder || !_pipeline) {
            LOGE("[Metal] prepareDraw: FAILED encoder={} pipeline={}", (void*)_renderEncoder, (void*)_pipeline.get());
            return false;
        }

        auto* mtlPipeline = dynamic_cast<MetalPipeline*>(_pipeline.get());
        if (!mtlPipeline) {
            LOGE("[Metal] prepareDraw: FAILED pipeline cast failed");
            return false;
        }

        MTLPixelFormat colorFmt = MTLPixelFormatBGRA8Unorm;
        if (_renderPassDescriptor && _renderPassDescriptor.colorAttachments[0].texture) {
            colorFmt = _renderPassDescriptor.colorAttachments[0].texture.pixelFormat;
        } else if (_currentDrawable) {
            colorFmt = _currentDrawable.texture.pixelFormat;
        }
        MTLPixelFormat depthFmt = MTLPixelFormatInvalid;
        if (_renderPassDescriptor && _renderPassDescriptor.depthAttachment.texture) {
            depthFmt = _renderPassDescriptor.depthAttachment.texture.pixelFormat;
        }
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

        static const uint32_t kMetalUniformBufferIndex = 8;
        for (uint32_t i = 0; i < _uniformBuffers.size(); ++i) {
            auto* buf = dynamic_cast<MetalBuffer*>(_uniformBuffers[i].get());
            if (buf && buf->handle()) {
                size_t uboOffset = (buf->type() == BufferType::Uniform) ? buf->submittedOffset() : 0;
                [_renderEncoder setVertexBuffer:(__bridge id<MTLBuffer>)buf->handle()
                                         offset:uboOffset
                                        atIndex:kMetalUniformBufferIndex + i];
                [_renderEncoder setFragmentBuffer:(__bridge id<MTLBuffer>)buf->handle()
                                           offset:uboOffset
                                          atIndex:kMetalUniformBufferIndex + i];
            }
        }

        return true;
    }

    std::shared_ptr<ISurface> _surface{};
    CAMetalLayer* _layer{nullptr};
    id<MTLDevice> __strong _device{nil};
    id<MTLCommandQueue> __strong _queue{nil};
    std::shared_ptr<ISwapchain> _swapchain{};
    std::shared_ptr<MetalSwapchain> _mtlSwapchain{};

    id<CAMetalDrawable> __strong _currentDrawable{nil};
    id<MTLCommandBuffer> __strong _commandBuffer{nil};
    id<MTLRenderCommandEncoder> __strong _renderEncoder{nil};
    MTLRenderPassDescriptor* __strong _renderPassDescriptor{nil};

    std::shared_ptr<IPipeline> _pipeline{};
    std::array<std::shared_ptr<IBuffer>, 8> _vertexBuffers{};
    std::vector<std::shared_ptr<IBuffer>> _uniformBuffers{};
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
