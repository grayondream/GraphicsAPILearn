#pragma once
#include <string>
#include <filesystem>

#define kResourceRoot "B:\\Code\\DirectX11Learn\\res\\"
namespace StaticCollector{
	inline std::filesystem::path getGLShaderPath() {
		return std::string(kResourceRoot) + "GL";
	}

	inline std::filesystem::path getDX11ShaderPath() {
		return std::string(kResourceRoot) + "DX11";
	}

	inline std::filesystem::path getDX12ShaderPath() {
		return std::string(kResourceRoot) + "DX12";
	}
};