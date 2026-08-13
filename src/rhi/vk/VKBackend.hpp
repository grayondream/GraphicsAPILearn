#pragma once
#include "rhi/core/IRenderer.hpp"

namespace rhi {

class VKTexture2D;
class VKTexture3D;

std::shared_ptr<IRenderer> createVKRenderer();

} // namespace rhi
