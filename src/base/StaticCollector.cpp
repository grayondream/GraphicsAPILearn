#include "StaticCollector.hpp"
#include "utils/FileUtils.hpp"
#include "rhi/core/Common.hpp"
#include <filesystem>
#ifndef RESOURCE_DIR
constexpr const char * kResourceRoot = "Res";
#else
constexpr const char* kResourceRoot = RESOURCE_DIR;
#endif//RESOURCE_DIR

namespace StaticCollector {
    std::string getResPath(){
		return kResourceRoot;
    }

    std::string getGLShaderPath() {
        if (rhi::backendKind() == rhi::BackendKind::Vulkan) {
            return getVulkanShaderPath();
        }
        return FileUtils::join(getResPath(), "GL");
	}

	std::string getVulkanShaderPath() {
		return FileUtils::join(getResPath(), "Vulkan");
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