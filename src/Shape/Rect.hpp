#pragma once

#include <array>
#include "Shape.hpp"
#include "Vertex.hpp"

class Rect : public Shape {
public:
	static constexpr int VertexCount = 4;

public:
	Rect() {
		store(
			{ {0.5f, 0.5f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} },
			{ {0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f} },
			{ {-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f} },
			{ {-0.5f, 0.5f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f} }
		);
	}

	Rect(const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex& v4) {
		store(v1, v2, v3, v4);
	}

private:
	void store(const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex& v4) {
		_pts.reserve(VertexCount);
		_pts = { v1, v2, v3, v4};
		_idx = { 0, 1, 2 , 2, 3, 0};
	}
};