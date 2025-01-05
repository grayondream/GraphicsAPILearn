#pragma once

#include <glm/glm.hpp>
#include <array>
#include "Shape.hpp"
#include "Vertex.hpp"

class Triangle : public Shape {
public:
	static constexpr int VertexCount = 3;

public:
	Triangle() {
		store(
			{ {0.0f,  0.5f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} },
			{ {0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f} },
			{ {-0.5f,-0.5f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f} }
		);
	}

	Triangle(const Vertex& v1, const Vertex& v2, const Vertex& v3) {
		store(v1, v2, v3);
	}

private:
	void store(const Vertex& v1, const Vertex& v2, const Vertex& v3) {
		_pts.reserve(VertexCount);
		_pts = { v1, v2, v3 };
		_idx = { 0, 1, 2 };
	}
};