#pragma once
#include "rhi/core/IRenderer.hpp"
#include <memory>
#include <string>

namespace RhiImage {
// 用 Image（stb）解码，上传为 RHI 2D 纹理
std::shared_ptr<rhi::ITexture2D> Load2D(rhi::IRenderer* renderer, const std::string& file);
// 从目录加载 right/left/top/bottom/front/back.jpg 六面，上传为 RHI cubemap
std::shared_ptr<rhi::ITexture3D> LoadCube(rhi::IRenderer* renderer, const std::string& dir);
}
