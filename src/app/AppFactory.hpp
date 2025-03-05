#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include "AppType.hpp"

class IApplication;
class AppFactory {
public:
    static std::shared_ptr<IApplication> create(const GraphicsType gtype, const AppType type);
};