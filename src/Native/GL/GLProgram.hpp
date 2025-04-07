#pragma once
#include <string>
#include <map>
#include <glm/glm.hpp>
#include <glad/glad.h>
class GLProgram {
public:
	~GLProgram();

public:
	bool init(const std::string vertFile = {}, const std::string fragFile = {});
	void use();
	
	GLuint id() const;

	GLint locate(const std::string &name) const;

	void destroy();

	const GLProgram& update(const std::string& name, const bool value) const;

	const GLProgram& update(const std::string& name, const int value) const;

	const GLProgram& update(const std::string& name, const float value) const;

	const GLProgram& update(const std::string& name, const float* value) const;

	const GLProgram& update(const std::string& name, const glm::vec3& value) const;
	
	const GLProgram& update(const std::string& name, const glm::mat4 &value) const;

	const GLProgram& update(const std::string& name, const glm::vec4& value) const;

private:
	std::pair<unsigned int, unsigned int> compileShader(const std::string vertFile, const std::string fragFile);
	GLuint createProgram(const std::string vertFile, const std::string fragFile);

private:
	GLuint _program{};
};

