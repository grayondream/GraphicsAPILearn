#pragma once
#include <string>
#include <filesystem>

namespace StaticCollector{
	std::filesystem::path getResPath();

	inline std::filesystem::path getGLShaderPath() {
		return getResPath() / "GL";
	}

	inline std::filesystem::path getDX11ShaderPath() {
		return getResPath() / "DX11";
	}

	inline std::filesystem::path getDX12ShaderPath() {
		return getResPath() / + "DX12";
	}

	inline std::filesystem::path getImagePath() {
		return getResPath() / +"img";
	}
};