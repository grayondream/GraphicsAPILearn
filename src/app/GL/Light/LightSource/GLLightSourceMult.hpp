#pragma once
#include "App/GL/Base/GLCameraBaseApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"
#include "Geometry/Sphere.hpp"
#include "Geometry/Cube.hpp"

class GLImageTexture2D;
class GLLightSourceMult : public GLCameraBaseApp {
public:
	virtual ~GLLightSourceMult();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void drawUI();
	void drawLight(const glm::mat4& proj, const glm::vec3& pos);
	void drawObjects(const glm::mat4& proj, const float curTime, const std::vector<glm::vec3>& pos);
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
	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
	std::shared_ptr<GLImageTexture2D> _objTex{};
	std::shared_ptr<GLImageTexture2D> _objBorderTex{};
};