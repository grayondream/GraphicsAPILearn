#include "Triangle.hpp"

Triangle::Triangle() {
    store(
        { 0.0f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f },
        { -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f }
    );
}

Triangle::Triangle(const Vertex& v1, const Vertex& v2, const Vertex& v3) {
    store(v1, v2, v3);
}

void Triangle::store(const Vertex& v1, const Vertex& v2, const Vertex& v3) {
    _pts[0] = v1;
    _pts[1] = v2;
    _pts[2] = v3;
    _idx = { 0, 1, 2 };
}

float* Triangle::data() const {
    return reinterpret_cast<float*>(const_cast<Vertex*>(_pts.data()));
}

 Triangle& Triangle::toGL()  {
    return *this;
}

 Triangle& Triangle::toDX11()  {
     _pts[0].z = -1 * _pts[0].z;
     _pts[1].z = -1 * _pts[1].z;
     _pts[2].z = -1 * _pts[2].z;
    return *this;
}

float* Triangle::idx() const {
    return reinterpret_cast<float*>(const_cast<unsigned int*>(_idx.data()));
}