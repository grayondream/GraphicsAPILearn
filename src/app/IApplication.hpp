#pragma once
#include <string>
#include "WindowProperties.hpp"

class IApplication {
public:
    IApplication() {}
    virtual ~IApplication() = default;

    virtual bool init(const GLFWWindowProperties& properties) = 0;

    virtual int run() = 0; 

    virtual void exit() = 0;

    virtual unsigned int getSampleCount() const { return 0; }
};