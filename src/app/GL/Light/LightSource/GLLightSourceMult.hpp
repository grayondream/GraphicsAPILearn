#pragma once
#include "App/GL/GLApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"
#include "Geometry/Sphere.hpp"
#include "Geometry/Cube.hpp"

class GLImageTexture2D;
class GLLightSourceMult : public GLApp {
public:
	virtual ~GLLightSourceMult();
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
	void drawUI();
	void drawLight(const glm::mat4& proj, const glm::vec3& pos);
	void drawObjects(const glm::mat4& proj, const float curTime, const glm::vec3& pos);
	void createVertexBuffer();
	void initProgram(const std::string name, GLProgram& program);
	std::shared_ptr<GLImageTexture2D> initTexture(const std::string img);

private:
	Cube _object{};
	GLProgram _targetProgram{};
	GLProgram _lightProgram{};
	unsigned int _vbo[3]{};
	unsigned int _vao{};
	unsigned int _ebo{};
	float _curTime{};
	Camera _camera{};
	bool _clicked{};
	bool _mouseClicked{ false };
	Point2D _lastPos{0.0, 0.0};
	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
	std::shared_ptr<GLImageTexture2D> _objTex{};
	std::shared_ptr<GLImageTexture2D> _objBorderTex{};
};