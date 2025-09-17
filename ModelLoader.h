#pragma once
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Mesh.h"
#include "SkinnedData.h"

bool IsSkinnedMesh(const aiScene* scene);
XMFLOAT3 SampleFloat3(const vector<pair<double, XMFLOAT3>>& keyframes, double t);
XMFLOAT4 SampleFloat4(const vector<pair<double, XMFLOAT4>>& keyframes, double t);
XMFLOAT4X4 AiToXmFloat4x4(const aiMatrix4x4& mat);

class ModelLoader
{
public:
    ModelLoader();
    ~ModelLoader() {}
    
    bool ReadModelFile(const char* fileName, float globalScale);
    // ReadModel이후 해당 모델에 쓸 애니메이션 할당

    template<typename VertexType, typename MeshContainerType>
    void ReadMesh(const aiScene* scene, MeshContainerType& meshContainer);

    void ReadNodeData(const aiScene* scene);
    void ReadBoneData(const aiScene* scene);

    bool ReadAnimationFile(const char* fileName, const string& animationName);
    void ReadAnimations(const aiScene* scene, const string& animationName);

    void InterpolateKeyframes(vector<Keyframe>& keyframes,
        const vector<pair<double, XMFLOAT3>>& positions,
        const vector<pair<double, XMFLOAT4>>& rotateQuats,
        const vector<pair<double, XMFLOAT3>>& scales);

    void Clear();
    
public:
    Assimp::Importer importer_;

    Mesh mesh_;
    SkinnedMesh skinnedMesh_;

    vector<Submesh> subMeshes_;

    SkinnedData skinnedData_;
};

