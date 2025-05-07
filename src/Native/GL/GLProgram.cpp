#include "GLProgram.hpp"
#include "Base/Log.hpp"
#include "Utils/FileUtils.hpp"
#include "glad/glad.h"
#include <Base/Assert.hpp>

static unsigned int GLCompileShader(const std::string file, const GLenum type) {
	std::string content = FileUtils::readFile2String(file);
	unsigned int shader = glCreateShader(type);
	const char* pcon = content.c_str();
	ASSERT(!content.empty(), "The file content should not be empty");
	glShaderSource(shader, 1, &pcon, NULL);
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

GLProgram::~GLProgram() {
}

GLuint GLProgram::uniformIndex(const std::string& name) const {
	return glGetUniformBlockIndex(id(), name.c_str());
}

const GLProgram& GLProgram::uniformBind(const std::string& name, const int binding) const {
	glUniformBlockBinding(id(), uniformIndex(name), binding);
	return *this;
}

void GLProgram::destroy() {
	if (_program != 0) {
		glDeleteProgram(_program);
	}
}

void GLProgram::use() {
	glUseProgram(_program);
}

bool GLProgram::init(const std::string vertFile, const std::string fragFile, const std::string geoFile) {
	_program = createProgram(vertFile, fragFile, geoFile);
	if (_program == GL_INVALID_INDEX) {
		return false;
	}

	return true;
}

GLint GLProgram::locate(const std::string& name) const {
	const auto loc = glGetUniformLocation(_program, name.c_str());
	//ASSERT(loc != -1, "Failed to locate {}", name);
	return loc;
}

GLuint GLProgram::id() const {
	return _program;
}

const GLProgram& GLProgram::update(const std::string& name, const bool value) const {
	glUniform1i(locate(name), value);
	return *this;
}

const GLProgram& GLProgram::update(const std::string& name, const int value)  const {
	glUniform1i(locate(name), value);
	return *this;
}

const GLProgram& GLProgram::update(const std::string& name, const float value) const {
	glUniform1f(locate(name), value);
	return *this;
}

const GLProgram& GLProgram::update(const std::string& name, const float* value) const {
	glUniformMatrix4fv(locate(name), 1, GL_FALSE, value);
	return *this;
}

const GLProgram& GLProgram::update(const std::string& name, const glm::mat4& value) const {
	glUniformMatrix4fv(locate(name), 1, GL_FALSE, &value[0][0]);
	return *this;
}

const GLProgram& GLProgram::update(const std::string& name, const glm::vec3& value) const {
	glUniform3fv(locate(name), 1, &value[0]);
	return *this;
}

const GLProgram& GLProgram::update(const std::string& name, const glm::vec4& value) const {
	glUniform4fv(locate(name), 1, &value[0]);
	return *this;
}

std::tuple<unsigned int, unsigned int, unsigned int> GLProgram::compileShader(const std::string vertFile, const std::string fragFile, const std::string geomFile) {
	if (vertFile.empty() || fragFile.empty()) {
		LOGI("Empty input for shader file!\nvertex file is {}\nfragment files is", vertFile, fragFile);
		return { GL_INVALID_INDEX ,GL_INVALID_INDEX, GL_INVALID_INDEX };
	}


	const auto vshader = GLCompileShader(vertFile, GL_VERTEX_SHADER);
	const auto fshader = GLCompileShader(fragFile, GL_FRAGMENT_SHADER);
	unsigned int gshader{};
	if (!geomFile.empty()) {
		gshader = GLCompileShader(geomFile, GL_GEOMETRY_SHADER);
	}

	return { vshader, fshader, gshader};
}

unsigned int GLProgram::createProgram(const std::string vertFile, const std::string fragFile, const std::string geomFile) {
	const auto [vshader, fshader, gshader] = compileShader(vertFile, fragFile, geomFile);
	if (vshader == GL_INVALID_INDEX || fshader == GL_INVALID_INDEX) {
		LOGE("Failed to compile shader from input file");
		return GL_INVALID_INDEX;
	}

	const auto program = glCreateProgram();
	glAttachShader(program, vshader);
	glAttachShader(program, fshader);
	if (gshader != GL_INVALID_INDEX) {
		glAttachShader(program, gshader);
	}
	
	glLinkProgram(program);
	int suc{};
	if (glGetProgramiv(program, GL_LINK_STATUS, &suc), !suc) {
		char buff[512]{};
		glGetProgramInfoLog(program, 512, NULL, buff);
		LOGE("Failed to Link Program!OpenGL Compile Error:\n {}", buff);
		return GL_INVALID_INDEX;
	}

	glDeleteShader(vshader);
	glDeleteShader(fshader);
	return program;
}
