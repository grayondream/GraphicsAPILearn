#pragma once

class Position2D{
public:
    using ValueType = float;
	static constexpr int ElemNo = 2;
    static constexpr int ByteSize = sizeof(ValueType) * ElemNo;
public:
    ValueType x,y;
};

class Position4D : public Position2D{
public:
	using ValueType = float;
	static constexpr int ElemNo = 4;
	static constexpr int ByteSize = sizeof(ValueType) * ElemNo;
public:
	Position4D& toGL() {
		return *this;
	}

	Position4D& toDX11() {
		z = -1 * z;
		return *this;
	}

public:
	ValueType z, w;
};