#pragma once
#include "geometry/Plane.hpp"

class GLPlane : public Plane{
public:
    int init();
    void destroy();
    unsigned int getVao() const { return vao_; }
private:
    unsigned int vao_;
    unsigned int vbo_[3];
};