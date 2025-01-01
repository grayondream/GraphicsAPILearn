#include "Triangle.hpp"

Triangle::Triangle() {
    Triangle({ 0, 0.5, 0.0 }, { - 0.5 , - 0.5, 0.0 }, { 0.5, -0.5, 0.0 });
}

Triangle::Triangle(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3) {
    _pts[0] = v1;
    _pts[1] = v2;
    _pts[2] = v3;
    _idx = { 0, 1, 2 };
}

float* Triangle::data() const {
    return reinterpret_cast<float*>(const_cast<glm::vec3*>(_pts.data()));
}

float* Triangle::idx() const {
    return reinterpret_cast<float*>(const_cast<unsigned int*>(_idx.data()));
}