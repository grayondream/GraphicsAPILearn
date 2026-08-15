#pragma once
#include "rhi/core/IRenderer.hpp"
#include <memory>

class Sample {
public:
    virtual ~Sample() = default;

    virtual bool load(std::shared_ptr<rhi::IRenderer> renderer) {
        _renderer = std::move(renderer);
        return true;
    }

    virtual void draw(float dt) {}
    virtual void renderBeforeLoop() {}
    virtual unsigned int getSampleCount() const { return 0; }

    virtual void onKeyPress(int key, int scancode, int action, int mods) {}
    virtual void onMouseMove(double x, double y) {}
    virtual void onMouseScroll(double xoffset, double yoffset) {}
    virtual void onMouseButton(int button, int action, int mods) {}
    virtual void onWindowResize(int width, int height) {}

    std::shared_ptr<rhi::IRenderer> renderer() const { return _renderer; }
    void setRenderer(std::shared_ptr<rhi::IRenderer> r) { _renderer = std::move(r); }
    float aspectRatio() const { return _aspect; }
    void setWindowSize(unsigned int width, unsigned int height) {
        _windowWidth = width;
        _windowHeight = height;
        if (height != 0) {
            _aspect = static_cast<float>(width) / static_cast<float>(height);
        }
    }
    unsigned int windowWidth() const { return _windowWidth; }
    unsigned int windowHeight() const { return _windowHeight; }

protected:
    std::shared_ptr<rhi::IRenderer> _renderer{};
    float _aspect{1.0f};
    unsigned int _windowWidth{0};
    unsigned int _windowHeight{0};
};
