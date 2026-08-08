#include "GLMultieInstanceApp.hpp"
#include "native/GL/GLProgram.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "geometry/Cube.hpp"
#include "native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;

using namespace ErrorHandle;

GLMultieInstanceApp::~GLMultieInstanceApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(2, _vbo);
		glDeleteBuffers(1, &_ebo);
	}

	_program.destroy();
}

bool GLMultieInstanceApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	{
		const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Sphere.vs");
		const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Sphere.fs");
		GLProgram program{};
		auto ret = program.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
		_program = program;
	}
	
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	createVertexBuffer();
	return true;
}

static unsigned int CreateObjectPositions(int count, int gap = 2.5){
	glm::vec2* translations = new glm::vec2[count * count];
    int index = 0;
    float offset = 0.1f;
	const int length = 2 * gap * (count - 1) / 2;
    for (int y = -length; y < length; y += 2 * gap)
    {
        for (int x = -length; x < length; x += 2 * gap)
        {
            glm::vec2 translation;
			translation.x = x;
			translation.y = y;
            translations[index++] = translation;
			LOGI("Append Position [{}, {}]", x, y);
        }
    }

	unsigned int instanceVBO;
    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * count * count, translations, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
	delete [] translations;
	return instanceVBO;
}

void GLMultieInstanceApp::createVertexBuffer() {
	unsigned int vbo[2]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(2, vbo);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
		
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 4));
		
		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, shape.normalSize(), shape.normal(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vector4DBase<float>), nullptr);
		
		_positionVbo = CreateObjectPositions(_count);
		glBindBuffer(GL_ARRAY_BUFFER, _positionVbo);
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    	glBindBuffer(GL_ARRAY_BUFFER, 0);
	    glVertexAttribDivisor(3, 1);

		// ������������
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.idxByteSize(), shape.idx(), GL_STATIC_DRAW);
	}
	glBindVertexArray(0);

	// ��¼ VBO �� EBO
	_vao = vao;
	_vbo[0] = vbo[0], _vbo[1] = vbo[1];
	_ebo = ebo;
}

void GLMultieInstanceApp::drawScene(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::End();

	glBindVertexArray(_vao);
	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	
	float radius = 5.0f; // 旋转半径
	glm::vec3 pos = glm::vec3(0.0,0.0, -3.0f);
	glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
	model = glm::translate(model, pos);
	model = glm::rotate(model, 0.f, glm::vec3(1.0f, 0.f, 0.f));
	const float scale = 0.3;
	model = glm::scale(model, glm::vec3(scale, scale, scale));
	//draw light source
	{
		_program.use();
		_program.update("projection", projection);
		_program.update("view", view);
		_program.update("model", model);
		_program.update("count", _count * _count);
		//glDrawElements(GL_TRIANGLES, shape.idxSize(), GL_UNSIGNED_INT, 0);
		//glDrawArrays(GL_TRIANGLES, 0, shape.idxSize() / 3);
		//glDrawArraysInstanced(GL_TRIANGLES, 0, shape.idxSize() / 3, 100);
		glDrawElementsInstanced(GL_TRIANGLES, shape.idxSize(), GL_UNSIGNED_INT, (void*)0, _count * _count);
	}
	glBindVertexArray(0);
	return GLApp::drawScene(dt);
}
