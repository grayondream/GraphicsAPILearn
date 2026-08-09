#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "native/GL/GLProgram.hpp"
#include "rhi/core/IPipeline.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include <vector>
#include "geometry/Sphere.hpp"

class GLImageTexture2D;
class Model;
class GLSaturnApp : public GLCameraBaseApp {

public:
	virtual ~GLSaturnApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void loadModel();
	void initSaturnPipeline();

private:
	std::shared_ptr<rhi::IPipeline> _saturnPipeline{};
	GLProgram _rockProgram{};
	std::shared_ptr<Model> _saturn;
	std::shared_ptr<Model> _rock;
	glm::vec3 _saturnPos{};
	std::vector<unsigned int> _rockVAOs{};
	std::vector<unsigned int> _rockIndexCounts{};
	unsigned int _vbo[2]{};
	unsigned int _vao{};
	unsigned int _ebo{};
	unsigned int _positionVbo{};
	int _count = 10;
	float _curTime{};
};