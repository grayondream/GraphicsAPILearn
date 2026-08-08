#pragma once
#include "app/AppType.hpp"
#include <memory>

class IApplication;
class GLAppFactory {
public:
    static std::shared_ptr<IApplication> create(const AppType type);
};