#pragma once
#include <string>
#include <filesystem>

#define kResourceRoot "B:\\Code\\DirectX11Learn\\res\\"
namespace StaticCollector{
	inline std::filesystem::path getResPath() {
		return std::string(kResourceRoot);
	}

	inline std::filesystem::path getGLShaderPath() {
		return getResPath() / "GL";
	}

	inline std::filesystem::path getDX11ShaderPath() {
		return getResPath() / + "DX11";
	}

	inline std::filesystem::path getDX12ShaderPath() {
		return getResPath() / + "DX12";
	}

	inline std::filesystem::path getImagePath() {
		return getResPath() / +"img";
	}
};