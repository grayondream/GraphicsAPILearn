#pragma once

#include <array>
#include "Shape.hpp"
#include "Vertex.hpp"

class Plane : public Shape {
public:
	Plane() {
        float planeVertices[] = {
            // positions          // texture Coords (note we set these higher than 1 (together with GL_REPEAT as texture wrapping mode). this will cause the floor texture to repeat)
             5.0f, -0.5f,  5.0f,  1.0f, 0.0f,
            -5.0f, -0.5f,  5.0f,  0.0f, 0.0f,
            -5.0f, -0.5f, -5.0f,  0.0f, 1.0f,
    
             5.0f, -0.5f,  5.0f,  1.0f, 0.0f,
            -5.0f, -0.5f, -5.0f,  0.0f, 1.0f,
             5.0f, -0.5f, -5.0f,  1.0f, 1.0f								
        };

		// Fill _pts and _uv
        for (size_t i = 0; i < sizeof(planeVertices) / sizeof(float); i += 5) {
            Vertex vertex;
            vertex.pos = { planeVertices[i], planeVertices[i + 1], planeVertices[i + 2], 1.0f };
            vertex.color = { 0.0f, 0.0f, 0.0f, 0.0f };
            _pts.push_back(vertex);
    
            Vector2DBase<float> uv = { planeVertices[i + 3], planeVertices[i + 4] };
            _uv.push_back(uv);
        }
	}
};