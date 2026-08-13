#pragma once
#include "app/AppType.hpp"
#include <memory>

class IApplication;
class VKAppFactory {
public:
    static std::shared_ptr<IApplication> create(const AppType type);
};
