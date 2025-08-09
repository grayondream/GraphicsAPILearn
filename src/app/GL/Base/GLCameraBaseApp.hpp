#pragma once
#include "App/GL/GLApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"

class GLImageTexture2D;
class GLCameraBaseApp : public GLApp {
public:
	virtual ~GLCameraBaseApp();

protected:
	virtual bool initApp() override;

	virtual void onMouseMove(double x, double y) override;
    virtual void onMouseScroll(double xoffset, double yoffset) override;
    virtual void onMouseButton(int button, int action, int mods) override;
    virtual void onKeyPress(int key, int scancode, int action, int mods) override;
    //virtual void onWindowResize(int width, int height) override;

protected:
	Camera _camera{};

private:
	bool _mouseClicked{ false };
	Point2D _lastPos{0.0, 0.0};
};