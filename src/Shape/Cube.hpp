#pragma once

#include <array>
#include "Shape.hpp"
#include "Vertex.hpp"

class Cube : public Shape<Cube, 8, 36> {
public:
	Cube() {
		_pts = {
			// Front face
		   Vertex{{-0.5f, -0.5f,  0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
		   Vertex{{ 0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
		   Vertex{{ 0.5f,  0.5f,  0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
		   Vertex{{-0.5f,  0.5f,  0.5f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},

		   // Back face
		   Vertex{{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
		   Vertex{{ 0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
		   Vertex{{ 0.5f,  0.5f, -0.5f, 1.0f}, {0.5f, 0.5f, 0.5f, 1.0f}},
		   Vertex{{-0.5f,  0.5f, -0.5f, 1.0f}, {1.0f, 0.5f, 0.5f, 1.0f}},
		};

		_idx = {
			0, 1, 2, 2, 3, 0, // Front face
			4, 5, 6, 6, 7, 4, // Back face
			0, 1, 5, 5, 4, 0, // Bottom face
			2, 3, 7, 7, 6, 2, // Top face
			0, 3, 7, 7, 4, 0, // Left face
			1, 2, 6, 6, 5, 1  // Right face}
		};
	}

};