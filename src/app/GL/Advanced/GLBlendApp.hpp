#pragma once
#include "App/GL/GLApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"

class GLImageTexture2D;
class GLBlendApp : public GLApp {
public:
	virtual ~GLBlendApp();
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
	void createTexture();
	void compileShader();
	void initGLEnv();

private:
	std::shared_ptr<GLImageTexture2D> _cubeTexture{};
	std::shared_ptr<GLImageTexture2D> _planeTexture{};
	std::shared_ptr<GLImageTexture2D> _grassTexture{};
	GLProgram _program{};
	unsigned int _cubeVbo[2]{};
	unsigned int _cubeVao{};
	unsigned int _planeVbo[2]{};
	unsigned int _planeVao{};
	float _curTime{};
	Camera _camera{};
	bool _clicked{};
	bool _mouseClicked{ false };
	Point2D _lastPos{ 0.0, 0.0 };
	glm::vec3 _objectPosition = glm::vec3(0, 0, -4.0f);
	glm::vec3 _objectScale = glm::vec3(20, 1, 20.0f);
	glm::vec3 _grassPos = glm::vec3(0.5f, 0.5f, 0.5f);
	int _grassCount;
};