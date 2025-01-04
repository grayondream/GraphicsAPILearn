#pragma once
#include <glm/glm.hpp>

class Vertex {
public:
	using ValueType = float;
	static constexpr int ElemNo = 4;
	static constexpr int ByteSize = sizeof(ValueType) * ElemNo * 2;
	static constexpr int ColorOffset = sizeof(ValueType) * ElemNo;

public:
	ValueType x, y, z, w;
	ValueType r, g, b, a;

public:
	Vertex& toGL(){
		return *this;
	}

	Vertex& toDX11(){
		z = -1 * z;
		return *this;
	}
};
