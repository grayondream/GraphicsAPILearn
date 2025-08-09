#include "AppFactory.hpp"
#if ENABLE_OPENGL
#include "App/GL/GLAppFactory.hpp"
#endif
#if ENABLE_DX11
#include "App/DX11/DX11AppFactory.hpp"
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
        default:
            return nullptr;
    }

    return nullptr;
}

