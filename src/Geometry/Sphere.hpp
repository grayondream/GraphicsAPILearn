#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cmath>
#include "Shape.hpp"
#include "Vertex.hpp"

class Sphere : public Shape {
public:
    Sphere(float radius = 1.0f, int sectorCount = 36, int stackCount = 18) {
        generate(_pts, _idx, _normal, radius, sectorCount, stackCount);
    }

private:
    static void generate(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, std::vector<Vector4DBase<float>>& normals,
        float radius, int sectorCount, int stackCount) {
        constexpr float PI = 3.14159265359f;

        vertices.clear();
        indices.clear();
        normals.clear();

        // Step 1: Generate vertices and normals
        for (int i = 0; i <= stackCount; ++i) {
            float stackAngle = PI / 2 - i * (PI / stackCount); // From PI/2 to -PI/2
            float xy = radius * cosf(stackAngle);
            float z = radius * sinf(stackAngle);

            for (int j = 0; j <= sectorCount; ++j) {
                float sectorAngle = j * (2 * PI / sectorCount);

                float x = xy * cosf(sectorAngle);
                float y = xy * sinf(sectorAngle);

                // Store the vertex
                Color color = { (x / radius + 1) / 2, (y / radius + 1) / 2, (z / radius + 1) / 2, 1.0f };
                vertices.emplace_back(Vertex{ {x, y, z, 1.0f}, color });

                // Calculate the normal for this vertex and normalize it
                //glm::vec3 normalVec = glm::normalize(glm::vec3(x, y, z));
                glm::vec3 normalVec = glm::vec3(x, y, z);
                normals.push_back(Vector4DBase<float>{normalVec.x, normalVec.y, normalVec.z, 1.0});
            }
        }

        // Step 2: Generate indices
        for (int i = 0; i < stackCount; ++i) {
            for (int j = 0; j < sectorCount; ++j) {
                int first = i * (sectorCount + 1) + j;
                int second = first + sectorCount + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }
    }
};
