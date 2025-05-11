#ifndef MESH_H
#define MESH_H

#include <glad/glad.h> // holds all OpenGL type declarations

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Native/GL/GLProgram.hpp>

#include <string>
#include <vector>
using namespace std;

#define MAX_BONE_INFLUENCE 4
using std::string;
using std::vector;

struct MeshVertex {
    // position
    glm::vec3 Position;
    // normal
    glm::vec3 Normal;
    // texCoords
    glm::vec2 TexCoords;
    // tangent
    glm::vec3 Tangent;
    // bitangent
    glm::vec3 Bitangent;
	//bone indexes which will influence this vertex
	int m_BoneIDs[MAX_BONE_INFLUENCE];
	//weights from each bone
	float m_Weights[MAX_BONE_INFLUENCE];
};

struct Texture {
    unsigned int id;
    string type;
    string path;
};

class Mesh {
public:
    // constructor
    Mesh(vector<MeshVertex> vertices, vector<unsigned int> indices, vector<Texture> textures);

    // render the mesh
    void draw(GLProgram &shader);

public:
    // mesh Data
    vector<MeshVertex>       vertices;
    vector<unsigned int> indices;
    vector<Texture>      textures;
private:
    // render data 
    unsigned int _vbo, _ebo;
    unsigned int _vao;
    // initializes all the buffer objects/arrays
    void setupMesh();
};
#endif
