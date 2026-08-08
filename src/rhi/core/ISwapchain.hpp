#pragma once
#include "Common.hpp"

namespace rhi {

class ISwapchain {
public:
    virtual ~ISwapchain() = default;
    virtual bool present() = 0;
    virtual void resize(int width, int height) = 0;
    virtual void* handle() = 0;   // 表面/原生句柄
};

} // namespace rhi
