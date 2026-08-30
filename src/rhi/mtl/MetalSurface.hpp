#pragma once

#include "rhi/core/ISurface.hpp"

typedef struct GLFWwindow GLFWwindow;

namespace rhi::mtl {

// 从 GLFW 窗口的 NSView 取出（必要时创建）CAMetalLayer，返回其指针。
// 实现位于 MetalSurface.mm（Objective-C++）。
void* createMetalLayer(GLFWwindow* window);

class MetalSurface : public ISurface {
public:
    explicit MetalSurface(void* layer, int width = 0, int height = 0)
        : _layer(layer), _width(width), _height(height) {}
    ~MetalSurface() override = default;

    void* nativeHandle() override { return _layer; }
    int width() const override { return _width; }
    int height() const override { return _height; }

    void* layer() const { return _layer; }

private:
    void* _layer{nullptr};
    int _width{0};
    int _height{0};
};

} // namespace rhi::mtl
