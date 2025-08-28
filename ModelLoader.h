#pragma once
#include "Mesh.h"
#include "include/cglft/cgltf.h"
#include "SkinnedData.h"

class ModelLoader
{
public:
    ModelLoader() {}
    ~ModelLoader() {}
    
    bool ReadModel(const char* filename);
    void ReadSkinedMesh(cgltf_data* data);

    void Clear();
private:
    Mesh mesh;
    SkinedMesh skinedMesh;
};

