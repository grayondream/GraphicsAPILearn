#pragma once
#include <string>
#include <map>
#include <tuple>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <vector>

class GLProgram {
public:
	~GLProgram();

public:
	bool init(const std::string vertFile, const std::string fragFile, const std::string geomFile = {});
	void use();
	
	GLuint id() const;

	GLint locate(const std::string &name) const;

	GLuint uniformIndex(const std::string& name) const;

	const GLProgram& uniformBind(const std::string& name, const int binding = 0) const;

	void destroy();

	const GLProgram& update(const std::string& name, const bool value) const;

	const GLProgram& update(const std::string& name, const int value) const;

	const GLProgram& update(const std::string& name, const float value) const;

	const GLProgram& update(const std::string& name, const float* value) const;

	const GLProgram& update(const std::string& name, const glm::vec3& value) const;
	
	const GLProgram& update(const std::string& name, const glm::vec4& value) const;

	const GLProgram& update(const std::string& name, const std::vector<glm::vec3>& value) const;

	const GLProgram& update(const std::string& name, const std::vector<glm::vec4>& value) const;

	const GLProgram& update(const std::string& name, const glm::mat4 &value) const;

private:
	std::tuple<unsigned int, unsigned int, unsigned int> compileShader(const std::string vertFile, const std::string fragFile, const std::string geomFile);
	GLuint createProgram(const std::string vertFile, const std::string fragFile, const std::string geomFile);

private:
	GLuint _program{};
};

