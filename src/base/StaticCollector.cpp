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
        if (rhi::backendKind() == rhi::BackendKind::Dx12) {
            // 返回 HLSL 镜像树根：DXShader 以源路径推导 build/res/DX12 下同名 .cso
            return getDX12ShaderPath();
        }
        if (rhi::backendKind() == rhi::BackendKind::Dx11) {
            // 返回 HLSL 镜像树根：DX11Shader 以源路径推导 build/res/DX11 下同名 .fxc
            return getDX11ShaderPath();
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