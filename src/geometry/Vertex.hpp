#pragma once

#include "base/Vector.hpp"
class Color {
public:
	using ValueType = float;
	static constexpr int ElemNo = 4;
	static constexpr int ByteSize = sizeof(ValueType) * ElemNo;
public:
	ValueType r, g, b, a;
};

class Vector4D : public Vector4DBase<float> {
public:
	using ValueType = float;
	static constexpr int ElemNo = 4;
	static constexpr int ByteSize = sizeof(ValueType) * ElemNo;
public:
	Vector4D(const std::initializer_list<ValueType>& ls)
		: Vector4DBase(ls){}

	Vector4D& toGL() {
		return *this;
	}

	Vector4D& toDX11() {
		z = -1 * z;
		return *this;
	}
};

class Vertex {
public:
	static constexpr int ByteSize = Vector4D::ByteSize + Color::ByteSize;
	static constexpr int ColorOffset = Vector4D::ByteSize;

public:
	Vertex() {
		pos = { 0.0, 0.0, 0.0, 0.0 };
		color = { 0.0, 0.0, 0.0, 0.0 };
	}

	Vertex(const Vector4D& pos, const Color& color) {
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
	Vector4D pos{ 0.0, 0.0, 0.0, 0.0 };
	Color color{ 0.0, 0.0, 0.0, 0.0 };
};
