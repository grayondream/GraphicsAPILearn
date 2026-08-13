#include "AppFactory.hpp"
#if ENABLE_OPENGL
#include "app/GL/GLAppFactory.hpp"
#endif
#if ENABLE_DX11
#include "app/DX11/DX11AppFactory.hpp"
#endif
#if ENABLE_VULKAN
#include "app/Vulkan/VKAppFactory.hpp"
#endif
#include "IApplication.hpp"

std::shared_ptr<IApplication> AppFactory::create(const GraphicsType gtype, const AppType type) {
    switch (gtype) {
        case GraphicsType::GL:
        #if ENABLE_OPENGL
            return GLAppFactory::create(type);
        #endif
            return nullptr;
        case GraphicsType::DX11:
        #if ENABLE_DX11
            return DX11AppFactory::create(type);
        #endif
            return nullptr;
        case GraphicsType::Vulkan:
        #if ENABLE_VULKAN
            return VKAppFactory::create(type);
        #endif
            return nullptr;
        default:
            return nullptr;
    }

    return nullptr;
}

