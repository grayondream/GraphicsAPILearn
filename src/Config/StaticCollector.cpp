#include "StaticCollector.hpp"
#include "Utils/FileUtils.hpp"
#include <filesystem>
#ifndef RESOURCE_DIR
constexpr const char * kResourceRoot = "res";
#else
constexpr const char* kResourceRoot = RESOURCE_DIR;
#endif//RESOURCE_DIR

namespace StaticCollector {
    std::string getResPath(){
        const auto path = std::filesystem::current_path().string();
        return FileUtils::join(path, kResourceRoot);
    }

    std::string getGLShaderPath() {
        return FileUtils::join(getResPath(), "GL");
	}

	std::string getDX11ShaderPath() {
		return FileUtils::join(getResPath(), "DX11");
	}

	std::string getDX12ShaderPath() {
		return FileUtils::join(getResPath(), "DX12");
	}

	std::string getImagePath() {
		return FileUtils::join(getResPath(), "img");
	}

	std::string getModelPath() {
		return FileUtils::join(getResPath(), "Model");
	}
}