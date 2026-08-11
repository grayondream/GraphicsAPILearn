#include "GLBackend.hpp"
#include "GLSwapchain.hpp"
#include "GLShader.hpp"
#include "GLPipeline.hpp"
#include "GLBuffer.hpp"
#include "GLTexture2D.hpp"
#include "GLTexture3D.hpp"
#include "GLRenderTarget.hpp"
#include "rhi/core/ISurface.hpp"
#include "base/Log.hpp"
#include "GLHeader.hpp"
#include <glm/glm.hpp>

namespace rhi {

static GLenum ToGLPrimitive(PrimitiveType t) {
    switch (t) {
        case PrimitiveType::TriangleStrip: return GL_TRIANGLE_STRIP;
        case PrimitiveType::Lines:         return GL_LINES;
        case PrimitiveType::Points:        return GL_POINTS;
        default:                           return GL_TRIANGLES;
    }
}

class GLRenderer final : public IRenderer {
public:
    bool init(const std::shared_ptr<ISurface>& surface) override {
        _surface = surface;
        _swapchain = std::make_shared<GLSwapchain>(
            static_cast<GLFWwindow*>(_surface->nativeHandle()));
        if (!_swapchain) return false;

        LOGI("OpenGL Vendor: {}", (char*)glGetString(GL_VENDOR));
        LOGI("OpenGL Renderer: {}", (char*)glGetString(GL_RENDERER));
        LOGI("OpenGL Version: {}", (char*)glGetString(GL_VERSION));
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        return true;
    }

    void shutdown() override {}

    std::shared_ptr<IShader> createShader() override { return std::make_shared<GLShader>(); }
    std::shared_ptr<IPipeline> createPipeline(const VertexLayout& layout, const std::shared_ptr<IShader>& shader) override {
        auto p = std::make_shared<GLPipeline>();
        auto gls = std::dynamic_pointer_cast<GLShader>(shader);
        p->bindShader(gls, layout);
        return p;
    }
    std::shared_ptr<IBuffer> createBuffer() override { return std::make_shared<GLBuffer>(); }
    std::shared_ptr<IBuffer> createUniformBuffer() override { return std::make_shared<GLBuffer>(); }
    std::shared_ptr<ITexture2D> createTexture2D() override { return std::make_shared<GLTexture2D>(); }
    std::shared_ptr<ITexture3D> createTexture3D() override { return std::make_shared<GLTexture3D>(); }
    std::shared_ptr<IRenderTarget> createRenderTarget() override { return std::make_shared<GLRenderTarget>(); }
    std::shared_ptr<ISwapchain> getSwapchain() override { return _swapchain; }

    void beginFrame() override {}
    void endFrame() override {}
    bool present() override { return _swapchain ? _swapchain->present() : false; }
    void clearColor(float r, float g, float b, float a) override {
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
    void setViewport(const Viewport& vp) override {
        _viewportW = vp.width;
        _viewportH = vp.height;
        glViewport(vp.x, vp.y, vp.width, vp.height);
    }
    void setPipeline(const std::shared_ptr<IPipeline>& pipeline) override {
        _pipeline = pipeline;
        if (pipeline) pipeline->use();
    }
    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer) override {
        if (_pipeline) _pipeline->setVertexBuffer(buffer, 0);
    }
    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer, uint32_t binding) override {
        if (_pipeline) _pipeline->setVertexBuffer(buffer, binding);
    }
    void setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) override {
        if (_pipeline) _pipeline->setIndexBuffer(buffer);
    }
    void setRenderTarget(const std::shared_ptr<IRenderTarget>& target) override {
        if (target) target->bind();
        else glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    void bindTexture(const std::shared_ptr<ITexture2D>& texture, unsigned int unit) override {
        if (texture) texture->bind(unit);
    }
    void bindTexture(const std::shared_ptr<ITexture3D>& texture, unsigned int unit) override {
        if (texture) texture->bind(unit);
    }
    void bindTexture(rhi::ITexture2D* texture, unsigned int unit) override {
        if (texture) texture->bind(unit);
    }
    void draw(uint32_t vertexCount, uint32_t firstVertex) override {
        const GLenum mode = _pipeline ? ToGLPrimitive(_pipeline->primitiveType()) : GL_TRIANGLES;
        glDrawArrays(mode, firstVertex, vertexCount);
    }
    void drawIndexed(uint32_t indexCount, uint32_t indexOffset, uint32_t vertexOffset) override {
        (void)vertexOffset;
        const GLenum mode = _pipeline ? ToGLPrimitive(_pipeline->primitiveType()) : GL_TRIANGLES;
        glDrawElements(mode, indexCount, GL_UNSIGNED_INT,
                       reinterpret_cast<void*>(indexOffset * sizeof(unsigned int)));
    }
    void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount,
                              uint32_t indexOffset, uint32_t vertexOffset) override {
        (void)vertexOffset;
        const GLenum mode = _pipeline ? ToGLPrimitive(_pipeline->primitiveType()) : GL_TRIANGLES;
        glDrawElementsInstanced(mode, indexCount, GL_UNSIGNED_INT,
                                reinterpret_cast<void*>(indexOffset * sizeof(unsigned int)), instanceCount);
    }
    void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) override {
        const GLenum mode = _pipeline ? ToGLPrimitive(_pipeline->primitiveType()) : GL_TRIANGLES;
        glDrawArraysInstanced(mode, firstVertex, vertexCount, instanceCount);
    }
    void blitFramebuffer(const std::shared_ptr<IRenderTarget>& src,
                         const std::shared_ptr<IRenderTarget>& dst,
                         BlitMask mask = static_cast<BlitMask>(static_cast<uint8_t>(BlitMask::Color) |
                                                               static_cast<uint8_t>(BlitMask::Depth))) override {
        if (!src) return;
        glBindFramebuffer(GL_READ_FRAMEBUFFER,
                          static_cast<GLuint>(reinterpret_cast<uintptr_t>(src->handle())));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                          dst ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(dst->handle())) : 0);
        GLbitfield bits = 0;
        if (static_cast<uint8_t>(mask) & static_cast<uint8_t>(BlitMask::Color)) bits |= GL_COLOR_BUFFER_BIT;
        if (static_cast<uint8_t>(mask) & static_cast<uint8_t>(BlitMask::Depth)) bits |= GL_DEPTH_BUFFER_BIT;
        glBlitFramebuffer(0, 0, _viewportW, _viewportH, 0, 0, _viewportW, _viewportH, bits, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    BackendCapabilities backendCapabilities() override {
        BackendCapabilities caps;
        GLint msaa = 0;
        glGetIntegerv(GL_MAX_SAMPLES, &msaa);
        caps.maxSamples = msaa;
        caps.maxUniformBlockSize = 16384;   // GL 最低保证 16KB
        return caps;
    }

private:
    std::shared_ptr<ISurface> _surface{};
    std::shared_ptr<ISwapchain> _swapchain{};
    std::shared_ptr<IPipeline> _pipeline{};
    int _viewportW{0}, _viewportH{0};
};

std::shared_ptr<IRenderer> createGLRenderer() {
    return std::make_shared<GLRenderer>();
}

} // namespace rhi
