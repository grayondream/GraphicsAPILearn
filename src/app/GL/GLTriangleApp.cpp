#include "GLTriangleApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Config/StaticCollectorPredefined.hpp"
#include "EH/ErrorHandle.hpp"

bool GLTriangleApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}

	const auto vfile = StaticCollector::getGLShaderPath() / "Shape" / "triangle.vert";
	const auto ffile = StaticCollector::getGLShaderPath() / "Shape" / "triangle.frag";
	auto ret = _program.init(vfile.string(), ffile.string());
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	return true;
}