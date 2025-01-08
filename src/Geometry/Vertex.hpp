#pragma once
#include "Math/Vector.hpp"

class Color {
public:
	using ValueType = float;
	static constexpr int ElemNo = 4;
	static constexpr int ByteSize = sizeof(ValueType) * ElemNo;
public:
	ValueType r, g, b, a;
};

using Position2D = Vector2D;

class Position4D : public Vector4D {
public:
	using ValueType = float;
	static constexpr int ElemNo = 4;
	static constexpr int ByteSize = sizeof(ValueType) * ElemNo;
public:
	Position4D(const std::initializer_list<ValueType>& ls)
		: Vector4D(ls){}

	Position4D& toGL() {
		return *this;
	}

	Position4D& toDX11() {
		z = -1 * z;
		return *this;
	}
};

class Vertex {
public:
	static constexpr int ByteSize = Position4D::ByteSize + Color::ByteSize;
	static constexpr int ColorOffset = Position4D::ByteSize;

public:
	Vertex() {
		pos = { 0.0, 0.0, 0.0, 0.0 };
		color = { 0.0, 0.0, 0.0, 0.0 };
	}

	Vertex(const Position4D& pos, const Color& color) {
		this->pos = pos;
		this->color = color;
	}
	

public:
	Vertex& toGL(){
		pos.toGL();
		return *this;
	}

	Vertex& toDX11(){
		pos.toDX11();
		return *this;
	}

public:
	Position4D pos{ 0.0, 0.0, 0.0, 0.0 };
	Color color{ 0.0, 0.0, 0.0, 0.0 };
};
