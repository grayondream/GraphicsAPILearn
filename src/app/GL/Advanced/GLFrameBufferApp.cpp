#include "GLFrameBufferApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Config/StaticCollector.hpp"
#include "EH/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Geometry/Cube.hpp"
#include <Geometry/Plane.hpp>
#include "Native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Base/Log.hpp"
#include "imgui.h"
#include "Geometry/Rect.hpp"

using namespace ErrorHandle;

GLFrameBufferApp::~GLFrameBufferApp() {
	if (_cubeVao != 0) {
		glDeleteVertexArrays(1, &_cubeVao);
		glDeleteBuffers(2, _cubeVbo);
	}

	glDeleteVertexArrays(1, &_planeVao);
	glDeleteBuffers(2, _planeVbo);

	glDeleteVertexArrays(1, &_screenVao);
	glDeleteBuffers(2, _screenVbo);
	glDeleteBuffers(1, &_screenEbo);

	glDeleteFramebuffers(1, &_screenFrameBuffer);
	glDeleteRenderbuffers(1, &_screenRbo);
	_contentProgram.destroy();
	_screenProgram.destroy();
}

GLProgram GLFrameBufferApp::compileShader(const std::string& name) {
	const auto vfile = StaticCollector::getGLShaderPath() / "Advanced" / "FrameBuffer" / (name + ".vert");
	const auto ffile = StaticCollector::getGLShaderPath() / "Advanced" / "FrameBuffer" / (name + ".frag");
	GLProgram program;
	auto ret = program.init(vfile.string(), ffile.string());
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	return program;
}

void GLFrameBufferApp::compileShader() {
	_contentProgram = compileShader("Basic");
	_screenProgram = compileShader("Screen");
}

void GLFrameBufferApp::loadTexture() {
	{
		const auto imgFile = StaticCollector::getImagePath() / "container2.jpg";
		_cubeTexture = std::make_shared<GLImageTexture2D>(imgFile.string());
		const auto valid = _cubeTexture->load().texture()->valid();
		ExitIfFailed(valid, "Failed to load texture from file {}", imgFile.string());
	}
	
	{
		const auto imgFile = StaticCollector::getImagePath() / "metal.jpg";
		_planeTexture = std::make_shared<GLImageTexture2D>(imgFile.string());
		const auto valid = _planeTexture->load().texture()->valid();
		ExitIfFailed(valid, "Failed to load texture from file {}", imgFile.string());
	}
}

void GLFrameBufferApp::initGLEnv() {
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glEnable(GL_DEPTH_TEST);
	//glDepthFunc(GL_LESS);
}

bool GLFrameBufferApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}

	_camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90, -10);
	compileShader();
	loadTexture();
	createCubeBuffer();
	createPlaneBuffer();
	createScreenBuffer();
	createFrameBuffer();
	initGLEnv();
	return true;
}

void GLFrameBufferApp::createFrameBuffer() {
	unsigned int frameBuffer{}, frameTexture{  };
	glGenFramebuffers(1, &frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	glGenTextures(1, &frameTexture);
	glBindTexture(GL_TEXTURE_2D, frameTexture);
	const auto attr = _attribute.winAttr;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, attr.width, attr.height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frameTexture, 0);

	unsigned int rbo{};
	glGenRenderbuffers(1, &rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, attr.width, attr.height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_FRAMEBUFFER, rbo);
	auto ret = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ErrorHandle::ExitIfFailed(ret == GL_FRAMEBUFFER_COMPLETE, "Failed to create frame buffer and bind it into render buffer");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	_screenFrameBuffer = frameBuffer;
	_screenTextureId = frameTexture;
	_screenRbo = rbo;
}

void GLFrameBufferApp::createCubeBuffer() {
	Cube shape{};
	unsigned int vbo[2]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(2, vbo);
	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 4));
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.idxByteSize(), shape.idx(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, shape.uvSize(), shape.uv(), GL_STATIC_DRAW);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		glEnableVertexAttribArray(2);
	}
	glBindVertexArray(0);
	_cubeVao = vao;
	_cubeVbo[0] = vbo[0], _cubeVbo[1] = vbo[1];
}

void GLFrameBufferApp::createPlaneBuffer() {
	Plane shape{};
	unsigned int vbo[2]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(2, vbo);
	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 4));
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.idxByteSize(), shape.idx(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, shape.uvSize(), shape.uv(), GL_STATIC_DRAW);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		glEnableVertexAttribArray(2);
	}
	glBindVertexArray(0);
	_planeVao = vao;
	_planeVbo[0] = vbo[0], _planeVbo[1] = vbo[1];
}

void GLFrameBufferApp::createScreenBuffer() {
	Rect shape{};
	unsigned int vbos[2]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(2, vbos);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
		glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
		glBufferData(GL_ARRAY_BUFFER, shape.uvSize(), shape.uv(), GL_STATIC_DRAW);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		glEnableVertexAttribArray(2);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.idxByteSize(), shape.idx(), GL_STATIC_DRAW);

	}
	glBindVertexArray(0);
	_screenVao = vao;
	_screenEbo = ebo;
	_screenVbo[0] = vbos[0];
	_screenVbo[1] = vbos[1];
}

void GLFrameBufferApp::clearColor() {
	return GLApp::clearColor();
}

void GLFrameBufferApp::beginDrawScene() {
	return GLApp::beginDrawScene();
}

static std::vector<glm::vec3> initializeCubePositions() {
	std::vector<glm::vec3> positions;
	float spacing = 1.1f; // ��������Ϊ 2.0f��ʹ�������������һ��

	for (int x = -2; x < 2; ++x) {
		for (int y = -2; y < 2; ++y) {
			for (int z = -2; z < 2; ++z) {
				positions.push_back(glm::vec3(x * spacing, y * spacing - 2, z * spacing - 5));
			}
		}
	}
	return positions;
}

void GLFrameBufferApp::drawPlane() {
	_planeTexture->texture()->bind(1);
	_contentProgram.use();
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	_contentProgram.update("projection", projection);

	const auto view = _camera.getViewMatrix();
	_contentProgram.update("view", view);
	glBindVertexArray(_planeVao);
	glm::mat4 model = glm::mat4(1.0f); // ��ʼ������Ϊ��λ����
	model = glm::translate(model, glm::vec3(-1.0, -4.50, -10)); // ƽ����������λ��
	_contentProgram.update("model", model); // ����ģ�;���
	_contentProgram.update("textureSampler", 1);
	glDrawArrays(GL_TRIANGLES, 0, 6); // ����������

	glBindVertexArray(0);
}

void GLFrameBufferApp::drawCube() {
	_cubeTexture->texture()->bind(0);
	_contentProgram.use();
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	_contentProgram.update("projection", projection);

	const auto view = _camera.getViewMatrix();
	_contentProgram.update("view", view);

	static float curTime = 0; // ���ֵ�ǰʱ��
	glBindVertexArray(_cubeVao);
	_contentProgram.update("textureSampler", 0);

	std::vector<glm::vec3> cubePositions = initializeCubePositions();
	int count = cubePositions.size(); // ����������
	for (int i = 0; i < count; i++) {
		glm::mat4 model = glm::mat4(1.0f); // ��ʼ������Ϊ��λ����
		model = glm::translate(model, cubePositions[i]); // ƽ����������λ��

		// ʹ�õ�ǰʱ�������������������ת�Ƕ�
		float angle = 0; // ÿ���������Բ�ͬ���ٶ���ת
		model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f)); // ��ת

		_contentProgram.update("model", model); // ����ģ�;���
		glDrawArrays(GL_TRIANGLES, 0, 36); // ����������
	}

	glBindVertexArray(0);
}

void GLFrameBufferApp::drawScene(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::SetNextItemWidth(200);
	//ImGui::SliderInt("Cube Count", &count, 1, 10);
	ImGui::End();
	
	glBindFramebuffer(GL_FRAMEBUFFER, _screenFrameBuffer);

	drawCube();
	drawPlane();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	clearColor();
	_screenProgram.use();
	glBindVertexArray(_screenVao);
	glBindTexture(GL_TEXTURE_2D, _screenTextureId);
	_screenProgram.update("textureSampler", (int)_screenTextureId);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	return GLApp::drawScene(dt);
}

void GLFrameBufferApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLFrameBufferApp::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	switch (msg) {
	case WM_KEYDOWN:
		break;
	case WM_KEYUP:
		break;
	case WM_CHAR:
		const char ch = static_cast<char>(wParam);
		switch (wParam) {
		case 'w':
			_camera.processKeyboardEvent(Camera::Movement::Forward, 0.5); break;
		case 's':
			_camera.processKeyboardEvent(Camera::Movement::Backward, 0.5); break;
		case 'd':
			_camera.processKeyboardEvent(Camera::Movement::Right, 0.5); break;
		case 'a':
			_camera.processKeyboardEvent(Camera::Movement::Left, 0.5); break;
		}
		break;
	}

	return GLApp::onKeyBoardEvent(msg, wParam, lParam);
}

void GLFrameBufferApp::onMouseDown(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONDOWN:
		_mouseClicked = true; break;
	}

	return GLApp::onMouseDown(msg, btnState, x, y);
}

void GLFrameBufferApp::onMouseUp(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONUP:
		_mouseClicked = false; break;
	}

	return GLApp::onMouseUp(msg, btnState, x, y);
}

void GLFrameBufferApp::onMouseMove(WPARAM btnState, int x, int y) {
	if (!_mouseClicked) {
		return GLApp::onMouseMove(btnState, x, y);
	}

	if (!_clicked) {
		_clicked = true;
		_lastPos = { (float)x, (float)y };
		return GLApp::onMouseMove(btnState, x, y);
	}

	const float offx = x - _lastPos.x;
	const float offy = y - _lastPos.y;
	_camera.processMouseMove(offx, offy);
	_lastPos = { (float)x, (float)y };
	return GLApp::onMouseMove(btnState, x, y);
}

void GLFrameBufferApp::onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	_camera.processMouseScrool(zDelta);
	return GLApp::onMouseScroll(msg, wParam, lParam);
}