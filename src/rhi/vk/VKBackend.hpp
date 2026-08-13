#pragma once
#include "rhi/core/IRenderer.hpp"

namespace rhi {

std::shared_ptr<IRenderer> createVKRenderer();

} // namespace rhi
