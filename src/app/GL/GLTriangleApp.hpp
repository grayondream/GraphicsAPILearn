#pragma once
#include "GLApp.hpp"
#include "Native/GL/GLProgram.hpp"

class GLTriangleApp : public GLApp {
public:
	virtual bool init(const HINSTANCE, const WindowDesc& param) override;

private:
	GLProgram _program{};
};