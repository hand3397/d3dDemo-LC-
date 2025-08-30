#pragma once
#include "Mesh.h"
#include "include/cgltf/cgltf.h"
#include "SkinnedData.h"

class ModelLoader
{
public:
    ModelLoader() {}
    ~ModelLoader() {}
    
    bool ReadModel(const char* filename);
    void ReadSkinedMesh(cgltf_data* data);

    void Clear();
public:
    Mesh mesh;
    SkinedMesh skinedMesh;

    vector<Submesh> submeshes;
};

