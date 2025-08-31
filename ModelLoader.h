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
    void ReadSkinnedMesh(cgltf_data* data);

    void Clear();
public:
    Mesh mesh;
    SkinnedMesh skinnedMesh;

    vector<Submesh> submeshes;
};

