#pragma once

namespace rhi {

// 抽象原生平台表面，由后端/窗口适配器提供。
struct ISurface {
    virtual ~ISurface() = default;
    virtual void* nativeHandle() = 0;  // GL 返回 GLFWwindow*；后续 DX11 返回 HWND
    virtual int width() const = 0;
    virtual int height() const = 0;
};

} // namespace rhi
