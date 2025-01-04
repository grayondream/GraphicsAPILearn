#pragma once

class IShape {
public:
	virtual  IShape& toGL()  = 0;
	virtual  IShape& toDX11()  = 0;
	virtual float* data() const = 0;
	virtual float* idx() const = 0;
	virtual std::size_t byteSize() = 0;
};