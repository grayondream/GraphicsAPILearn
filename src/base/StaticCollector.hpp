#pragma once
#include <string>

namespace StaticCollector{
	std::string getResPath();

	std::string getGLShaderPath();

	std::string getVulkanShaderPath();

	std::string getDX11ShaderPath();

	std::string getDX12ShaderPath();

	std::string getImagePath();

	std::string getModelPath();
};