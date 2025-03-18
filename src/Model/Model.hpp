#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <Native/GL/GLProgram.hpp>
#include <Model/GLMesh.hpp>

class Model {
public:
    /*  函数   */
    Model(const char *path){
        loadModel(path);
    }
    void draw(const GLProgram& shader);   
private:
    /*  函数   */
    void loadModel(const std::string path);
    void processNode(aiNode *node, const aiScene *scene);
    Mesh::Mesh processMesh(aiMesh *mesh, const aiScene *scene);
    std::vector<Mesh::Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type,  const std::string typeName);

private:
    /*  模型数据  */
    std::vector<Mesh::Texture> textures_loaded;
    std::vector<Mesh::Mesh> meshes;
    std::string directory;
};