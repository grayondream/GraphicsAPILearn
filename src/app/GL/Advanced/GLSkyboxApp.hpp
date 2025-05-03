
#pragma once
#include "App/GL/GLApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"

class GLImageTexture2D;
class GLImageTexture3D;
class GLSkyboxApp : public GLApp {
public:
	virtual ~GLSkyboxApp();
public:
	virtual bool init(const HINSTANCE, const WindowDesc& param) override;

protected:
	virtual void clearColor();
	virtual void beginDrawScene();
	virtual void drawScene(const float dt);
	virtual void endDrawScene();

	virtual void onMouseDown(const UINT msg, WPARAM btnState, int x, int y);
	virtual void onMouseUp(const UINT msg, WPARAM btnState, int x, int y);
	virtual void onMouseMove(WPARAM btnState, int x, int y);
	virtual void onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam);
	virtual void onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) override;

private:
	void createVertexBuffer();
	void drawCube();
	void drawSkybox();

private:
	std::shared_ptr<GLImageTexture2D> _texture{};
	std::shared_ptr<GLImageTexture3D> _skyBoxTexture{};
	GLProgram _program{};
	GLProgram _skyboxProgram{};
	unsigned int _vbo[3]{};
	unsigned int _vao{};
	unsigned int _skyVao{};
	unsigned int _skyVbo{};
	float _curTime{};
	Camera _camera{};
	bool _clicked{};
	bool _mouseClicked{ false };
	bool _enableReflect{};
	bool _enableRefraction{};
	Point2D _lastPos{0.0, 0.0};
};