#pragma once
#include <string>
#include <Windows.h>
#include "WindowProperties.hpp"

class IApplication {
public:
    IApplication() {}
    virtual ~IApplication() = default;

    virtual bool init(const GLFWWindowProperties& properties) = 0;

    virtual int run() = 0; 

    virtual void exit() = 0;
};