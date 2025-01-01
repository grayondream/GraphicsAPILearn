#include "AppRegister.hpp"
#include "Application.hpp"
#if ENABLE_OPENGL
#include "App/GL/GLAppRegister.hpp"
#endif
#if ENABLE_DX11
#include "App/DX11/DX11AppRegister.hpp"
#endif
AppRegister::AppRegister() {}

std::shared_ptr<IApplication> AppRegister::get(const std::string &name){
    const auto it = _apps.find(name);
    if(it != _apps.end()){
        return it->second;
    }

    return {};
}

void AppRegister::push(const std::string &name, const std::shared_ptr<IApplication> &app){
    _apps[name] = app;
}

AppRegister* AppRegister::instance() {
    static AppRegister re;
    return &re;
}

void AppRegister::run() {
    AppRegister::instance()->push("Base", std::make_shared<Application>());
#if ENABLE_OPENGL
    RegisterGLApps();
#endif

#if ENABLE_DX11
    RegisterDX11Apps();
#endif

#if ENABLE_DX12
#endif
}
