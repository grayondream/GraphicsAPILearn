#pragma once
#include <glm/glm.hpp>

class Position{
public:
	using ValueType = float;
	static constexpr int ElemNo = 4;
	static constexpr int ByteSize = sizeof(ValueType) * ElemNo;
public:
	Position& toGL() {
		return *this;
	}

	Position& toDX11() {
		z = -1 * z;
		return *this;
	}

public:
	ValueType x, y, z, w;
};

class Color {
public:
	using ValueType = float;
	static constexpr int ElemNo = 4;
	static constexpr int ByteSize = sizeof(ValueType) * ElemNo;
public:
	ValueType r, g, b, a;
};

class Vertex {
public:
	static constexpr int ByteSize = Position::ByteSize + Color::ByteSize;
	static constexpr int ColorOffset = Position::ByteSize;

public:
	Position pos{};
	Color color{};

public:
	Vertex& toGL(){
		pos.toGL();
		return *this;
	}

	Vertex& toDX11(){
		pos.toDX11();
		return *this;
	}
};
