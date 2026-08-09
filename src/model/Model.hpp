#ifndef MODEL_H
#define MODEL_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <model/Mesh.hpp>
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderer.hpp"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
using namespace std;

class Model 
{
public:
    // constructor, expects a filepath to a 3D model.
    Model(rhi::IRenderer* renderer, string const &path, bool gamma = false);

    // draws the model, and thus all its meshes
    void draw(rhi::IRenderer* renderer, rhi::IPipeline* pipeline, int count = 1);

    // vertex input layout shared by the model's meshes
    const rhi::VertexLayout& vertexLayout() const;

private:
    // loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
    void loadModel(string const &path);

    // processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
    void processNode(aiNode *node, const aiScene *scene);

    Mesh processMesh(aiMesh *mesh, const aiScene *scene);

    // checks all material textures of a given type and loads the textures if they're not loaded yet.
    // the required info is returned as a Texture struct.
    vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName);
public:
    // model data 
    vector<Texture> textures_loaded;	// stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
    vector<Mesh>    meshes;
    string directory;
    bool gammaCorrection;
private:
    rhi::IRenderer* _renderer{nullptr};
};

#endif
