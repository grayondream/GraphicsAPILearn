#pragma once

#include <array>
#include "Vertex.hpp"
#include <vector>

class Shape {
public:
	using ValueType = Vertex;
public:
	Shape() {};
	virtual ~Shape() {}

public:
	const float* data() const {
		return reinterpret_cast<float*>(const_cast<Vertex*>(_pts.data()));
	}

	const unsigned int* idx() const {
		return _idx.data();
	}

	Shape& toGL() {
		for (int i = 0; i < size(); i++) {
			_pts[i].toGL();
		}
		return *this;
	}

	Shape& toDX11() {
		for (int i = 0; i < size(); i++) {
			_pts[i].toDX11();
		}
		return *this;
	}
	
	std::size_t byteSize() {
		return _pts.size() * sizeof(Vertex);
	}

	std::size_t idxByteSize() {
		return _idx.size() * sizeof(unsigned int);
	}

	std::size_t size() {
		return _pts.size();
	}

protected:
	std::vector<Vertex> _pts; // 三个顶点
	std::vector<unsigned int> _idx; // 索引数组
};