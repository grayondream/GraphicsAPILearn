#pragma once

class IShape {
public:
	virtual float* data() const = 0;
	virtual float* idx() const = 0;
	virtual std::size_t byteSize() = 0;
};