#pragma once
#include "app/sample/Sample.hpp"
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include <memory>

class BaseSample : public Sample {
public:
    virtual ~BaseSample() = default;

    virtual bool load(std::shared_ptr<rhi::IRenderer> renderer) override;
    virtual void onMouseMove(double x, double y) override;
    virtual void onMouseScroll(double xoffset, double yoffset) override;
    virtual void onMouseButton(int button, int action, int mods) override;
    virtual void onKeyPress(int key, int scancode, int action, int mods) override;

protected:
    Camera _camera{};
private:
    bool _mouseClicked{ false };
    Point2D _lastPos{ 0.0, 0.0 };
};
