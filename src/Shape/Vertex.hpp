#pragma once
#include "Position.hpp"

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
	static constexpr int ByteSize = Position4D::ByteSize + Color::ByteSize;
	static constexpr int ColorOffset = Position4D::ByteSize;

public:
	Position4D pos{};
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
