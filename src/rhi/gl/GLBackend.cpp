#include "GLBackend.hpp"
#include "GLSwapchain.hpp"
#include "GLShader.hpp"
#include "GLPipeline.hpp"
#include "GLBuffer.hpp"
#include "GLTexture2D.hpp"
#include "GLTexture3D.hpp"
#include "GLRenderTarget.hpp"
#include "rhi/core/ISurface.hpp"
#include "GLHeader.hpp"
#include <glm/glm.hpp>

namespace rhi {

class GLRenderer final : public IRenderer {
public:
    bool init(const std::shared_ptr<ISurface>& surface) override {
        _surface = surface;
        _swapchain = std::make_shared<GLSwapchain>(
            static_cast<GLFWwindow*>(_surface->nativeHandle()));
        return _swapchain != nullptr;
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
    void setViewport(const Viewport& vp) override { glViewport(vp.x, vp.y, vp.width, vp.height); }
    void setPipeline(const std::shared_ptr<IPipeline>& pipeline) override {
        if (pipeline) pipeline->use();
    }
    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer) override {
        if (buffer) buffer->bind();
    }
    void setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) override {
        if (buffer) buffer->bind();
    }
    void bindTexture(const std::shared_ptr<ITexture2D>& texture, unsigned int unit) override {
        if (texture) texture->bind(unit);
    }
    void draw(uint32_t vertexCount, uint32_t firstVertex) override {
        glDrawArrays(GL_TRIANGLES, firstVertex, vertexCount);
    }
    void drawIndexed(uint32_t indexCount, uint32_t indexOffset, uint32_t vertexOffset) override {
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, reinterpret_cast<void*>(indexOffset * sizeof(unsigned int)));
    }

private:
    std::shared_ptr<ISurface> _surface{};
    std::shared_ptr<ISwapchain> _swapchain{};
};

std::shared_ptr<IRenderer> createGLRenderer() {
    return std::make_shared<GLRenderer>();
}

} // namespace rhi
