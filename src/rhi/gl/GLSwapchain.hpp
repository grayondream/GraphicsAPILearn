#pragma once
#include "rhi/core/ISwapchain.hpp"
#include "GLHeader.hpp"

namespace rhi {

class GLSwapchain : public ISwapchain {
public:
    explicit GLSwapchain(GLFWwindow* window);
    bool present() override;
    void resize(int width, int height) override;
    void* handle() override;

private:
    GLFWwindow* _window{nullptr};
};

} // namespace rhi
