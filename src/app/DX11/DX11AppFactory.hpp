#pragma once
#include "app/AppType.hpp"
#include <memory>

class DX11App;
class DX11AppFactory{
public:
    static std::shared_ptr<DX11App> create(const AppType type);
};  
