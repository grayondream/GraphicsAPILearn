#pragma once

#include "geometry/Sphere.hpp"


class GLSphere : public Sphere{
public:
    int init();
    void destroy();
    unsigned int getVao() const { return vao_; }
private:
    unsigned int vbos_[3]{};
    unsigned int vao_{}, ebo_{};
};