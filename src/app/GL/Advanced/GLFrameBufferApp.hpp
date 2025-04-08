#pragma once
#include "App/GL/GLApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"

class GLImageTexture2D;
class GLFrameBufferApp : public GLApp {
public:
	virtual ~GLFrameBufferApp();
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
	void createCubeBuffer();
	void createPlaneBuffer();
	void createScreenBuffer();
	void createFrameBuffer();
	void compileShader();
	void loadTexture();
	void initGLEnv();
	void drawPlane();
	void drawCube();
	GLProgram compileShader(const std::string& name);

private:
	std::shared_ptr<GLImageTexture2D> _cubeTexture{};
	std::shared_ptr<GLImageTexture2D> _planeTexture{};
	GLProgram _contentProgram{};
	GLProgram _screenProgram{};
	unsigned int _cubeVbo[2]{};
	unsigned int _cubeVao{};
	unsigned int _planeVbo[2]{};
	unsigned int _planeVao{};
	unsigned int _screenVao{};
	unsigned int _screenVbo[2];
	unsigned int _screenEbo{};
	unsigned int _screenFrameBuffer{};
	unsigned int _screenTextureId{};
	unsigned int _screenRbo{};
	float _curTime{};
	Camera _camera{};
	bool _clicked{};
	bool _mouseClicked{ false };
	Point2D _lastPos{ 0.0, 0.0 };
};