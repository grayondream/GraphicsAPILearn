#pragma once

#include <glm/glm.hpp>
#include <array>
#include "IShape.hpp"
#include "Vertex.hpp"

class Triangle : public IShape {
public:
	static constexpr int VertexCount = 3;
public:
	Triangle();
	Triangle(const Vertex& v1, const Vertex& v2, const Vertex& v3);

public:
	virtual float* data() const override;
	virtual  Triangle& toGL()  override;
	virtual  Triangle& toDX11()  override;
	virtual float* idx() const override;
	virtual std::size_t byteSize() override {
		return VertexCount * sizeof(Vertex);
	}

private:
	void store(const Vertex& v1, const Vertex& v2, const Vertex& v3);

private:
	std::array<Vertex, VertexCount> _pts; // 三个顶点
	std::array<unsigned int, VertexCount> _idx; // 索引数组
};