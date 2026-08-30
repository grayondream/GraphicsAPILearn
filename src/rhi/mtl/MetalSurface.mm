#include "rhi/mtl/MetalSurface.hpp"

#import <QuartzCore/QuartzCore.h>
#import <Cocoa/Cocoa.h>

#include <GLFW/glfw3.h>
#if defined(__APPLE__)
#    define GLFW_EXPOSE_NATIVE_COCOA
#    include <GLFW/glfw3native.h>
#endif

namespace rhi::mtl {

void* createMetalLayer(GLFWwindow* window) {
    if (window == nullptr) return nullptr;
    NSWindow* nswin = (__bridge NSWindow*)glfwGetCocoaWindow(window);
    if (nswin == nullptr) return nullptr;
    NSView* view = [nswin contentView];
    if (view == nullptr) return nullptr;

    CAMetalLayer* layer = nil;
    if ([view.layer isKindOfClass:[CAMetalLayer class]]) {
        layer = (CAMetalLayer*)view.layer;
    } else {
        layer = [CAMetalLayer layer];
        [layer setContentsScale:view.window.backingScaleFactor];
        view.layer = layer;
        view.wantsLayer = YES;
    }
    layer.frame = view.bounds;
    layer.contentsScale = view.window.backingScaleFactor;
    return (__bridge void*)layer;
}

} // namespace rhi::mtl
