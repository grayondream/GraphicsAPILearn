#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <Native/GL/GLProgram.hpp>

namespace Mesh {
struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
};

class Mesh {
public:
    /*  函数  */
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const std::vector<Texture>& textures);

public:
    void draw(const GLProgram &shader);

private:
    /*  渲染数据  */
    /*  函数  */
    void setupMesh();

public:
    /*  网格数据  */
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    
private:
    unsigned int VAO, VBO, EBO;
}; 

}