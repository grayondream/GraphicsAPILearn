#pragma once
#include "rhi/core/ISurface.hpp"
#include "GLHeader.hpp"

namespace rhi {

class GLFWSurface : public ISurface {
public:
    explicit GLFWSurface(GLFWwindow* window, int w, int h) : _window(window), _w(w), _h(h) {}
    void* nativeHandle() override { return _window; }
    int width() const override { return _w; }
    int height() const override { return _h; }

private:
    GLFWwindow* _window{nullptr};
    int _w{0}, _h{0};
};

} // namespace rhi
