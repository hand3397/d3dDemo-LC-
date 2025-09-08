#pragma once
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Mesh.h"
#include "SkinnedData.h"

bool IsSkinnedMesh(const aiScene* scene);
XMFLOAT3 SampleFloat3(const vector<pair<float, XMFLOAT3>>& keyframes, float t);
XMFLOAT4 SampleFloat4(const vector<pair<float, XMFLOAT4>>& keyframes, float t);
XMFLOAT4X4 AiToXmFloat4x4(const aiMatrix4x4& mat);

class ModelLoader
{
public:
    ModelLoader() {}
    ~ModelLoader() {}
    
    bool ReadModel(const char* fileName);
    // ReadModel이후 해당 모델에 쓸 애니메이션 할당

    template<typename VertexType, typename MeshContainerType>
    void ReadMesh(const aiScene* scene, MeshContainerType& meshContainer);

    void ReadNodeData(const aiScene* scene);
    void ReadBoneData(const aiScene* scene);

    AnimationClip InterpolateAnimaitonClip(const vector<vector<pair<float, XMFLOAT3>>>& scaleAnimation, 
        const vector<vector<pair<float, XMFLOAT4>>>& rotateAnimation, 
        const vector<vector<pair<float, XMFLOAT3>>>& posAnimation);

    void Clear();
    
public:
    Mesh mesh_;
    SkinnedMesh skinnedMesh_;

    vector<Submesh> submeshes_;

    SkinnedData skinnedData_;
};

