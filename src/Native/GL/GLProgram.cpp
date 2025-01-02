#include "gl/gl.h"
#include "glad/glad.h"
#include "GLProgram.hpp"
#include "Base/Log.hpp"
#include "Utils/FileUtils.hpp"

static unsigned int GLCompileShader(const std::string file, const GLenum type) {
	std::string content = FileUtils::readFile2String(file);
	unsigned int shader = glCreateShader(type);
	glShaderSource(shader, 1, (const GLchar* const*)content.c_str(), NULL);
	glCompileShader(shader);
	int suc{};
	if (glGetShaderiv(shader, GL_COMPILE_STATUS, &suc), !suc) {
		LOGE("Failed to compile shader from file {}", file);
		char buff[512]{};
		glGetShaderInfoLog(shader, 512, NULL, buff);
		LOGE("OpenGL Compile Error:\n {}", buff);
		return GL_INVALID_INDEX;
	}

	return shader;
}

bool GLProgram::init(const std::string vertFile = {}, const std::string fragFile = {}) {
	_program = createProgram(vertFile, fragFile);
}

std::pair<unsigned int, unsigned int> GLProgram::compileShader(const std::string vertFile, const std::string fragFile) {
	if (vertFile.empty() || fragFile.empty()) {
		LOGI("Empty input for shader file!\nvertex file is {}\nfragment files is", vertFile, fragFile);
		return { GL_INVALID_INDEX ,GL_INVALID_INDEX };
	}


	const auto vshader = GLCompileShader(vertFile, GL_VERTEX_SHADER);
	const auto fshader = GLCompileShader(fragFile, GL_FRAGMENT_SHADER);
	return { vshader, fshader };
}

unsigned int GLProgram::createProgram(const std::string vertFile, const std::string fragFile) {
	const auto [vshader, fshader] = compileShader(vertFile, fragFile);
	if (vshader == GL_INVALID_INDEX || fshader == GL_INVALID_INDEX) {
		LOGE("Failed to compile shader from input file");
		return GL_INVALID_INDEX;
	}

	const auto program = glCreateProgram();
	glAttachShader(program, vshader);
	glAttachShader(program, fshader);
	glLinkProgram(program);
	int suc{};
	if (glGetProgramiv(program, GL_LINK_STATUS, &suc), !suc) {
		char buff[512]{};
		glGetShaderInfoLog(program, 512, NULL, buff);
		LOGE("Failed to Link Program!OpenGL Compile Error:\n {}", buff);
		return GL_INVALID_INDEX;
	}

	glDeleteShader(vshader);
	glDeleteShader(fshader);
	return program;
}
