#pragma once

#include <array>
#include "Shape.hpp"
#include "Vertex.hpp"

class Plane : public Shape {
    public:
        Plane() {
            float planeVertices[] = {
                // positions 3           // normals 3        // texcoords 2
                5.0f, -0.5f,  5.0f,  0.0f, 1.0f, 0.0f,  5.0f,  0.0f,
                -5.0f, -0.5f,  5.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
                -5.0f, -0.5f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,
        
                 5.0f, -0.5f,  5.0f,  0.0f, 1.0f, 0.0f,  5.0f,  0.0f,
                -5.0f, -0.5f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,
                 5.0f, -0.5f, -5.0f,  0.0f, 1.0f, 0.0f,  5.0f, 5.0f							
            };
    
            // Fill _pts and _uv
            for (size_t i = 0; i < sizeof(planeVertices) / sizeof(float); i += 8) {
                Vertex vertex;
                vertex.pos = { planeVertices[i], planeVertices[i + 1], planeVertices[i + 2], 1.0f };
                vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 颜色
                auto normal = { planeVertices[i + 3], planeVertices[i + 4], planeVertices[i + 5], 1.0f }; // 法线
                _normal.push_back(normal);
                _pts.push_back(vertex);
    
                Vector2DBase<float> uv = { planeVertices[i + 6], planeVertices[i + 7] };
                _uv.push_back(uv);
            }
        }
    };