#include "GLSwapchain.hpp"

namespace rhi {

GLSwapchain::GLSwapchain(GLFWwindow* window) : _window(window) {}

bool GLSwapchain::present() {
    if (!_window) return false;
    glfwSwapBuffers(_window);
    return true;
}

void GLSwapchain::resize(int width, int height) {
    glViewport(0, 0, width, height);
}

void* GLSwapchain::handle() {
    return _window;
}

} // namespace rhi
