#pragma once

struct GLFWWindowProperties {
    GLFWWindowProperties(
        const std::string& title = "Hello Window",
        unsigned int width = 1280,
        unsigned int height = 720,
        int xPos = 100,
        int yPos = 100,
        bool vsync = true,
        bool fullscreen = false
    ) : title(title), width(width), height(height),
        xPos(xPos), yPos(yPos), vsync(vsync), fullscreen(fullscreen) {}

    std::string title;
    unsigned int width;
    unsigned int height;
    int xPos;
    int yPos;
    bool vsync;
    bool fullscreen;
};