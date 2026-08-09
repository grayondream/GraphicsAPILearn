#ifndef MESH_H
#define MESH_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "rhi/core/IBuffer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/VertexLayout.hpp"

#include <cstddef>
#include <memory>
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
    std::shared_ptr<rhi::ITexture2D> texture;
    string type;
    string path;
};

class Mesh {
public:
    // constructor
    Mesh(rhi::IRenderer* renderer, vector<MeshVertex> vertices, vector<unsigned int> indices, vector<Texture> textures);

    // render the mesh
    void draw(rhi::IRenderer* renderer, rhi::IPipeline* pipeline, int count);

    // vertex input layout for this mesh
    const rhi::VertexLayout& layout() const { return _layout; }

public:
    // mesh Data
    vector<MeshVertex>       vertices;
    vector<unsigned int> indices;
    vector<Texture>      textures;

private:
    // render data
    rhi::VertexLayout _layout{};
    std::shared_ptr<rhi::IBuffer> _vb{};
    std::shared_ptr<rhi::IBuffer> _ib{};
    // initializes all the buffer objects/arrays
    void setupMesh(rhi::IRenderer* renderer);
};
#endif
