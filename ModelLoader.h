#pragma once
#include "Mesh.h"
#include "include/cgltf/cgltf.h"
#include "SkinnedData.h"
#

class ModelLoader
{
public:
    ModelLoader() {}
    ~ModelLoader() {}
    
    bool ReadModel(const char* fileName);
    // ReadModel이후 해당 모델에 쓸 애니메이션 할당
    bool ReadAnimation(const char* fileName, string animationName = "");

    void ReadSkinnedMesh(const cgltf_data* data);
    void ReadSkinnedData(const cgltf_data* data);
    void ReadAnimationClip(const cgltf_data* data, string& animationName);

    AnimationClip InterpolateAnimaitonClip(const vector<vector<pair<float, XMFLOAT3>>>& scaleAnimation, 
        const vector<vector<pair<float, XMFLOAT4>>>& rotateAnimation, 
        const vector<vector<pair<float, XMFLOAT3>>>& posAnimation);

    void Clear();
public:
    Mesh mesh;
    SkinnedMesh skinnedMesh;

    vector<Submesh> submeshes;

    SkinnedData skinnedData;
};

