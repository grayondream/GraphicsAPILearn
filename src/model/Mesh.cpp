#include "Mesh.hpp"

Mesh::Mesh(rhi::IRenderer* renderer, vector<MeshVertex> vertices, vector<unsigned int> indices, vector<Texture> textures){
    this->vertices = vertices;
    this->indices = indices;
    this->textures = textures;

    // now that we have all the required data, set the vertex buffers and its attribute pointers.
    setupMesh(renderer);
}

void Mesh::draw(rhi::IRenderer* renderer, rhi::IPipeline* pipeline, int count) {
    // bind appropriate textures
    unsigned int diffuseNr  = 1;
    unsigned int specularNr = 1;
    unsigned int normalNr   = 1;
    unsigned int heightNr   = 1;
    for(unsigned int i = 0; i < textures.size(); i++)
    {
        // retrieve texture number (the N in diffuse_textureN)
        string number;
        string name = textures[i].type;
        if(name == "texture_diffuse")
            number = std::to_string(diffuseNr++);
        else if(name == "texture_specular")
            number = std::to_string(specularNr++); // transfer unsigned int to string
        else if(name == "texture_normal")
            number = std::to_string(normalNr++); // transfer unsigned int to string
            else if(name == "texture_height")
            number = std::to_string(heightNr++); // transfer unsigned int to string

        // and finally bind the texture
        if (textures[i].texture) {
            renderer->bindTexture(textures[i].texture, i);
        }
    }

    // draw mesh
    renderer->setVertexBuffer(_vb);
    renderer->setIndexBuffer(_ib);
    if (count > 1) {
        renderer->drawIndexedInstanced(static_cast<unsigned int>(indices.size()), static_cast<unsigned int>(count));
    } else {
        renderer->drawIndexed(static_cast<unsigned int>(indices.size()));
    }
}

void Mesh::setupMesh(rhi::IRenderer* renderer){
    const int stride = static_cast<int>(sizeof(MeshVertex));

    _layout.elements = {
        { rhi::VertexElement::Float3, 0, 0, rhi::VertexInputRate::PerVertex, 0,                                  stride },
        { rhi::VertexElement::Float3, 1, 0, rhi::VertexInputRate::PerVertex, static_cast<int>(offsetof(MeshVertex, Normal)),     stride },
        { rhi::VertexElement::Float2, 2, 0, rhi::VertexInputRate::PerVertex, static_cast<int>(offsetof(MeshVertex, TexCoords)),   stride },
        { rhi::VertexElement::Float3, 3, 0, rhi::VertexInputRate::PerVertex, static_cast<int>(offsetof(MeshVertex, Tangent)),     stride },
        { rhi::VertexElement::Float3, 4, 0, rhi::VertexInputRate::PerVertex, static_cast<int>(offsetof(MeshVertex, Bitangent)),   stride },
        { rhi::VertexElement::Int4,   5, 0, rhi::VertexInputRate::PerVertex, static_cast<int>(offsetof(MeshVertex, m_BoneIDs)),   stride },
        { rhi::VertexElement::Float4, 6, 0, rhi::VertexInputRate::PerVertex, static_cast<int>(offsetof(MeshVertex, m_Weights)),   stride },
    };

    _vb = renderer->createBuffer();
    _ib = renderer->createBuffer();
    _vb->init(vertices.data(), vertices.size() * sizeof(MeshVertex), rhi::BufferType::Vertex);
    _ib->init(indices.data(), indices.size() * sizeof(unsigned int), rhi::BufferType::Index);
}
