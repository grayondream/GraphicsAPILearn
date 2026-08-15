#pragma once
#include "Common.hpp"
#include <memory>
#include <cstdint>

namespace rhi {

class IBuffer;
class IShader;
class IPipeline;
class ITexture2D;
class ITexture3D;
class IRenderTarget;
class ISurface;
class ISwapchain;
struct VKImGuiInitInfo;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // 生命周期
    virtual bool init(const std::shared_ptr<ISurface>& surface) = 0;
    virtual void shutdown() = 0;

    // 资源创建工厂（App 通过它获取资源）
    virtual std::shared_ptr<IShader> createShader() = 0;
    virtual std::shared_ptr<IPipeline> createPipeline(const VertexLayout& layout, const std::shared_ptr<IShader>& shader) = 0;
    virtual std::shared_ptr<IBuffer> createBuffer() = 0;
    virtual std::shared_ptr<IBuffer> createUniformBuffer() = 0;               // 新增
    virtual std::shared_ptr<ITexture2D> createTexture2D() = 0;
    virtual std::shared_ptr<ITexture3D> createTexture3D() = 0;
    virtual std::shared_ptr<IRenderTarget> createRenderTarget() = 0;
    virtual std::shared_ptr<ISwapchain> getSwapchain() = 0;

    // 帧控制
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual bool present() = 0;

    // 状态与绘制
    virtual void clearColor(float r, float g, float b, float a) = 0;
    virtual void setViewport(const Viewport& vp) = 0;
    virtual void setPipeline(const std::shared_ptr<IPipeline>& pipeline) = 0;
    virtual void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer) = 0;                                  // 保留（binding 0）
    virtual void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer, uint32_t binding) = 0;                // 新增：多 binding
    virtual void setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) = 0;
    virtual void setRenderTarget(const std::shared_ptr<IRenderTarget>& target) = 0;  // 新增：null=默认 framebuffer
    virtual void bindTexture(const std::shared_ptr<ITexture2D>& texture, unsigned int unit = 0) = 0;
    virtual void bindTexture(const std::shared_ptr<ITexture3D>& texture, unsigned int unit = 0) = 0;          // 新增：cubemap
    virtual void bindTexture(rhi::ITexture2D* texture, unsigned int unit = 0) = 0;     // 新增：raw 指针（RT 颜色纹理）
    virtual void draw(uint32_t vertexCount, uint32_t firstVertex = 0) = 0;
    virtual void drawIndexed(uint32_t indexCount, uint32_t indexOffset = 0, uint32_t vertexOffset = 0) = 0;
    virtual void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount,
                                      uint32_t indexOffset = 0, uint32_t vertexOffset = 0) = 0;              // 新增
    virtual void drawInstanced(uint32_t vertexCount, uint32_t instanceCount,
                               uint32_t firstVertex = 0) = 0;                                                // 新增
    virtual void blitFramebuffer(const std::shared_ptr<IRenderTarget>& src,
                                 const std::shared_ptr<IRenderTarget>& dst,
                                 BlitMask mask = static_cast<BlitMask>(static_cast<uint8_t>(BlitMask::Color) |
                                                                       static_cast<uint8_t>(BlitMask::Depth))) = 0;  // 新增：MSAA resolve / 拷贝
    virtual BackendCapabilities backendCapabilities() = 0;                                                    // 新增

    // ImGui / overlay 扩展钩子（默认 no-op 内联实现；VKRenderer 覆写，GL 后端不受影响）
    virtual bool imguiInitInfo(VKImGuiInitInfo& out) { (void)out; return false; }
    virtual void renderImGuiDrawData(void* drawData) { (void)drawData; }
};

} // namespace rhi
