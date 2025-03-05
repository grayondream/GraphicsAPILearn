#include "AppFactory.hpp"
#if ENABLE_OPENGL
#include "App/GL/GLAppFactory.hpp"
#endif
#if ENABLE_DX11
#include "App/DX11/DX11AppFactory.hpp"
#endif
#include "IApplication.hpp"
#include "DX11/DX11App.hpp"
#include "GL/GLApp.hpp"

std::shared_ptr<IApplication> AppFactory::create(const GraphicsType gtype, const AppType type) {
    switch (gtype) {
        case GraphicsType::GL:
            return GLAppFactory::create(type);
        case GraphicsType::DX11:
            return DX11AppFactory::create(type);
        default:
            return nullptr;
    }

    return nullptr;
}

