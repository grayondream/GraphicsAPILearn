#pragma once
#include "App/AppType.hpp"
#include <memory>

class GLApp;
class GLAppFactory {
public:
    static std::shared_ptr<GLApp> create(const AppType type);
};