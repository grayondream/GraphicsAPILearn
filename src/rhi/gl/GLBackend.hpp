#pragma once
#include "rhi/core/IRenderer.hpp"

namespace rhi {

// 顶层工厂：创建 GL 渲染器
std::shared_ptr<IRenderer> createGLRenderer();

} // namespace rhi
