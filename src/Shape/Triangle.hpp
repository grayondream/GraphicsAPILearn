#pragma once

#include <glm/glm.hpp>
#include <array>
#include "IShape.hpp"

class Triangle : public IShape {
public:
	Triangle();
	Triangle(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3);

public:
	virtual float* data() const override;
	virtual float* idx() const override;

private:
	std::array<glm::vec3, 3> _pts; // 三个顶点
	std::array<unsigned int, 3> _idx; // 索引数组
};