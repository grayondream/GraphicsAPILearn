#include "GLBackend.hpp"
#include "GLSwapchain.hpp"
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

    std::shared_ptr<IShader> createShader() override { return nullptr; }
    std::shared_ptr<IPipeline> createPipeline(const VertexLayout&, const std::shared_ptr<IShader>&) override { return nullptr; }
    std::shared_ptr<IBuffer> createBuffer() override { return nullptr; }
    std::shared_ptr<ITexture2D> createTexture2D() override { return nullptr; }
    std::shared_ptr<ITexture3D> createTexture3D() override { return nullptr; }
    std::shared_ptr<IRenderTarget> createRenderTarget() override { return nullptr; }
    std::shared_ptr<ISwapchain> getSwapchain() override { return _swapchain; }

    void beginFrame() override {}
    void endFrame() override {}
    bool present() override { return _swapchain ? _swapchain->present() : false; }
    void clearColor(float, float, float, float) override {}
    void setViewport(const Viewport&) override {}
    void setPipeline(const std::shared_ptr<IPipeline>&) override {}
    void setVertexBuffer(const std::shared_ptr<IBuffer>&) override {}
    void setIndexBuffer(const std::shared_ptr<IBuffer>&) override {}
    void bindTexture(const std::shared_ptr<ITexture2D>&, unsigned int) override {}
    void draw(uint32_t, uint32_t) override {}
    void drawIndexed(uint32_t, uint32_t, uint32_t) override {}

private:
    std::shared_ptr<ISurface> _surface{};
    std::shared_ptr<ISwapchain> _swapchain{};
};

std::shared_ptr<IRenderer> createGLRenderer() {
    return std::make_shared<GLRenderer>();
}

} // namespace rhi
