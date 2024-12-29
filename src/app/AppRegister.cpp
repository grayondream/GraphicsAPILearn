#include "AppRegister.hpp"
#include "App/GL/GLAppRegister.hpp"
#include "App/DX11/DX11AppRegister.hpp"

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
#if ENABLE_OPENGL
    RegisterGLApps();
#endif

#if ENABLE_DX11

#endif

#if ENABLE_DX12
#endif
}
