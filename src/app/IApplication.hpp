#pragma once
#include <string>
#include <Windows.h>

class IApplication {
public:
    struct WindowAttribute {
        int width;
        int height;
        std::string title{};

        WindowAttribute(int w = 800, int h = 600, const std::string& t = "Application")
            : width(w), height(h), title(t) {}
    };
    struct WindowDesc {
    public:
        WindowAttribute winAttr;
        bool enableMssa;
    };
public:
    IApplication() {}
    virtual ~IApplication() = default;

    virtual bool init(const HINSTANCE, const WindowDesc& param) = 0;
    virtual int run(const int nShowCmd) = 0; 
};