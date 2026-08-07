#pragma once

#include "Geometry/Cube.hpp"

class GLCube : public Cube{
public:
    int init();
    void destroy();
    unsigned int getVao() const { return vao_; }
private:
    unsigned int vbos_[3]{};
    unsigned int vao_{}, ebo_{};
};