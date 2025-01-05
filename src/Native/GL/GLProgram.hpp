#pragma once
#include <string>
#include <map>

class GLProgram {
public:
	~GLProgram();

public:
	bool init(const std::string vertFile = {}, const std::string fragFile = {});
	void use();
	
	GLProgram& update(const std::string& name, const bool value);

	GLProgram& update(const std::string& name, const int value);

	GLProgram& update(const std::string& name, const float value);

private:
	std::pair<unsigned int, unsigned int> compileShader(const std::string vertFile, const std::string fragFile);
	unsigned int createProgram(const std::string vertFile, const std::string fragFile);

private:
	unsigned int _program{};
};

