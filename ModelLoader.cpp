#include "ModelLoader.h"

bool IsSkinnedMesh(const aiScene* scene)
{
    for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[i];
        if (mesh->mNumBones > 0) {
            return true; // 스킨 정보가 있는 메시 존재 → SkinnedMesh
        }
    }
    return false; // 모든 메시가 mNumBones == 0 → 일반 메시
}

bool ModelLoader::ReadModel(const char* fileName)
{
    //this->Clear();

    Assimp::Importer importer;

    // 모델 파일 로드 (예: soldier.fbx, model.gltf, model.obj 등)
    const aiScene* scene = importer.ReadFile(fileName,
        aiProcess_Triangulate |        // 삼각형으로 변환
        aiProcess_FlipUVs |            // UV 좌표 뒤집기 (DirectX용)
        aiProcess_CalcTangentSpace |   // Normal / Tangent 계산
        aiProcess_JoinIdenticalVertices
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        //std::cerr << "Assimp Error: " << importer.GetErrorString() << std::endl;
        return false;
    }

    if (IsSkinnedMesh(scene)) {
        ReadMesh<SkinnedVertex>(scene, skinnedMesh_);
        ReadNodeData(scene);
        ReadBoneData(scene);
    }
    else {
        ReadMesh<Vertex>(scene, mesh_);
    }
    //ReadSkinnedData(scene);

    return true;
}

template<typename VertexType, typename MeshContainerType>
void ModelLoader::ReadMesh(const aiScene* scene, MeshContainerType& meshContainer)
{
    uint32_t baseVertex = 0;
    uint32_t baseIndex = 0;
    uint32_t numIndices = 0;

    for (uint32_t mi = 0; mi < scene->mNumMeshes; ++mi) {
        aiMesh* mesh = scene->mMeshes[mi];

        float floatMax = MathHelper::TypeMax<float>();
        float floatMin = MathHelper::TypeMin<float>();

        aiVector3D minVec(floatMax, floatMax, floatMax);
        aiVector3D maxVec(floatMin, floatMin, floatMin);

        uint32_t numVertices = mesh->mNumVertices;
        meshContainer.vertices.reserve(meshContainer.vertices.size() + numVertices);
        for (uint32_t vi = 0; vi < numVertices; vi++) {
            VertexType v;
            v.Pos = { mesh->mVertices[vi].x, mesh->mVertices[vi].y, mesh->mVertices[vi].z };

            minVec = { std::min(minVec.x, v.Pos.x), std::min(minVec.y, v.Pos.y), std::min(minVec.z, v.Pos.z) };
            maxVec = { std::max(maxVec.x, v.Pos.x), std::max(maxVec.y, v.Pos.y), std::max(maxVec.z, v.Pos.z) };

            if (mesh->HasNormals())
                v.Normal = { mesh->mNormals[vi].x, mesh->mNormals[vi].y, mesh->mNormals[vi].z };

            if (mesh->mTextureCoords[0]) {
                v.TexC = { mesh->mTextureCoords[0][vi].x, mesh->mTextureCoords[0][vi].y };
            }
            else {
                v.TexC = { 0.0f, 0.0f };
            }
            meshContainer.vertices.push_back(v);
        }

        meshContainer.indices.reserve(meshContainer.indices.size() + mesh->mNumFaces * 3);
        for (uint32_t fi = 0; fi < mesh->mNumFaces; fi++) {
            const aiFace& face = mesh->mFaces[fi];
            for (uint32_t i = 0; i < face.mNumIndices; i++) {
                meshContainer.indices.emplace_back(face.mIndices[i]);
            }
        }    

        BoundingBox boundingBox = { {(minVec.x + maxVec.x) / 2.0f, (minVec.y + maxVec.y) / 2.0f, (minVec.z + maxVec.z) / 2.0f},
            {(maxVec.x - minVec.x) / 2.0f, (maxVec.y - minVec.y) / 2.0f, (maxVec.z - minVec.z) / 2.0f} };
        numIndices = mesh->mNumFaces * 3;
        submeshes_.push_back({ numIndices , baseIndex , baseVertex , boundingBox });
        baseVertex += mesh->mNumVertices;
        baseIndex += numIndices;
    }
}

void ModelLoader::ReadNodeData(const aiScene* scene)
{
    unordered_map<string, uint32_t> nameToIdx;

    queue<aiNode*> q;
    q.push(scene->mRootNode);

    while (!q.empty()) {
        aiNode* node = q.front(); q.pop();
        nameToIdx.insert({ node->mName.C_Str(), nameToIdx.size() });

        for (uint32_t ci = 0; ci < node->mNumChildren; ci++)
            q.push(node->mChildren[ci]);
    }

    vector<uint32_t> parentNode(nameToIdx.size());
    vector<vector<uint32_t>> childrenNode(nameToIdx.size());
    vector<XMFLOAT4X4> nodeTransforms(nameToIdx.size());

    q.push(scene->mRootNode);
    while (!q.empty()) {
        aiNode* node = q.front(); q.pop();
        uint32_t nodeIdx = nameToIdx[node->mName.C_Str()];

        if (node->mParent != nullptr)
            parentNode[nodeIdx] = nameToIdx[node->mParent->mName.C_Str()];
        else
            parentNode[nodeIdx] = nodeIdx;

        nodeTransforms[nodeIdx] = AiToXmFloat4x4(node->mTransformation);

        for (uint32_t ci = 0; ci < node->mNumChildren; ci++) {
            childrenNode[nodeIdx].push_back(nameToIdx[node->mChildren[ci]->mName.C_Str()]);
            q.push(node->mChildren[ci]);
        }
    }

    skinnedData_.SetNode(parentNode, childrenNode, nameToIdx, nodeTransforms);
}

void ModelLoader::ReadBoneData(const aiScene* scene)
{
    /*
    for (int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* me = scene->mMeshes[i];
        for (int j = 0; j < me->mNumBones; j++) {
            aiBone* bo = me->mBones[j];
            int a = 1;
        }
    }
    */
    uint32_t numVertices = skinnedMesh_.vertices.size();
    vector<vector<pair<float, uint32_t>>> boneData(numVertices);
    unordered_map<uint32_t, uint32_t> nodeToBone;
    vector<XMFLOAT4X4> boneOffsets;
    
    uint32_t numMeshes = scene->mNumMeshes;
    for (uint32_t mi = 0; mi < numMeshes; mi++) {
        aiMesh* mesh = scene->mMeshes[mi];
        uint32_t numBones = mesh->mNumBones;

        uint32_t baseVertex = submeshes_[mi].BaseVertexLocation;

        for (uint32_t bi = 0; bi < numBones; bi++) {
            aiBone* bone = mesh->mBones[bi];
            int32_t nodeIdx = skinnedData_.NameToIdx(bone->mName.C_Str());
            // 노드에 저장되지 않은 bone은 무시
            if (nodeIdx == -1)
                continue;

            if (nodeToBone.find(nodeIdx) == nodeToBone.end()) {
                nodeToBone.insert({ nodeIdx, nodeToBone.size() });
                boneOffsets.push_back(AiToXmFloat4x4(bone->mOffsetMatrix));
            }

            uint32_t boneIdx = nodeToBone[nodeIdx];

            for (uint32_t wi = 0; wi < bone->mNumWeights; wi++) {
                aiVertexWeight vw = bone->mWeights[wi];
                boneData[vw.mVertexId + baseVertex].push_back({ vw.mWeight, boneIdx });
            }
        }
    }

    skinnedData_.SetBone(nodeToBone, boneOffsets);

    //boneData의 boneindex와 boneWeight를 skinnedVertex에 적재하기.
    for (uint32_t vi = 0; vi < numVertices; vi++) {
        vector<pair<float, uint32_t>>& weights = boneData[vi];
        uint32_t numWeights = weights.size();

        if (numWeights == 0)
            continue;

        // weight가 4개보다 많더라도 가장 영향을 많이 미치는 4개의 bone만 선택
        sort(weights.begin(), weights.end(), [](const pair<float, uint32_t>& a, const pair<float, uint32_t>& b) {
            return a.first > b.first; });

        uint32_t boneIndices[4] = { 0, 0, 0, 0 };
        float boneWeight[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        
        float sumWeight = 0.0f;
        for (int i = 0; i < 4 && i < numWeights; i++) {
            boneIndices[i] = weights[i].second;
            boneWeight[i] = weights[i].first;
            sumWeight += boneWeight[i];
        }

        // boneWeight 정규화
        if (sumWeight > 0.0f) {
            for (int i = 0; i < 4; i++) {
                boneWeight[i] /= sumWeight;
            }
        }

        SkinnedVertex& v = skinnedMesh_.vertices[vi];
        for (int i = 0; i < 4; i++)
            v.BoneIndices[i] = boneIndices[i];
        v.BoneWeights = { boneWeight[0], boneWeight[1], boneWeight[2] };
    }
}
/*
void ModelLoader::ReadBoneHeritage(const aiScene* scene)
{
    
}
*/

AnimationClip ModelLoader::InterpolateAnimaitonClip(const vector<vector<pair<float, XMFLOAT3>>>& scaleAnimation,
    const vector<vector<pair<float, XMFLOAT4>>>& rotateAnimation, 
    const vector<vector<pair<float, XMFLOAT3>>>& posAnimation)
{
    AnimationClip animationClip;

    int boneCount = skinnedData_.BoneCount();
    animationClip.boneAnimations_.resize(boneCount);

    for (int bi = 0; bi < boneCount; bi++) {
        auto& scaleAnim = scaleAnimation[bi];
        auto& rotateAnim = rotateAnimation[bi];
        auto& posAnim = posAnimation[bi];

        vector<float> times;
        times.reserve(scaleAnim.size() + rotateAnim.size() + posAnim.size());

        for (auto& kv : scaleAnim)  times.push_back(kv.first);
        for (auto& kv : rotateAnim) times.push_back(kv.first);
        for (auto& kv : posAnim)    times.push_back(kv.first);

        sort(times.begin(), times.end());

        auto it = std::unique(times.begin(), times.end(), MathHelper::FloatEqual());
        times.erase(it, times.end());

        // time마다 보간된 keyframe 구하기
        for (float t : times) {
            XMFLOAT3 s = SampleFloat3(scaleAnim, t);
            XMFLOAT4 r = SampleFloat4(rotateAnim, t);
            XMFLOAT3 p = SampleFloat3(posAnim, t);

            Keyframe kf;
            kf.timePos_ = t;
            kf.scale_ = s;
            kf.rotationQuat_ = r;
            kf.translation_ = p;

            animationClip.boneAnimations_[bi].keyframes_.push_back(kf);
        }
    }

    animationClip.SetClipTime();

    return animationClip;
}

void ModelLoader::Clear()
{
    mesh_.clear();
    skinnedMesh_.clear();
    skinnedData_ = SkinnedData();
    submeshes_.clear();
}

XMFLOAT3 SampleFloat3(const vector<pair<float, XMFLOAT3>>& keyframes, float t)
{
    if (keyframes.empty()) return XMFLOAT3(1, 1, 1);
    if (t <= keyframes.front().first) return keyframes.front().second;
    if (t >= keyframes.back().first)  return keyframes.back().second;

    // t를 감싸는 두 키프레임 찾기
    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        float t0 = keyframes[i].first;
        float t1 = keyframes[i + 1].first;
        if (t0 <= t && t <= t1) {
            float alpha = (t - t0) / (t1 - t0);
            XMFLOAT3 s0 = keyframes[i].second;
            XMFLOAT3 s1 = keyframes[i + 1].second;
            // 선형 보간
            return XMFLOAT3(
                s0.x + (s1.x - s0.x) * alpha,
                s0.y + (s1.y - s0.y) * alpha,
                s0.z + (s1.z - s0.z) * alpha
            );
        }
    }

    return keyframes.back().second;
}

XMFLOAT4 SampleFloat4(const vector<pair<float, XMFLOAT4>>& keyframes, float t)
{
    if (keyframes.empty()) return XMFLOAT4(0, 0, 0, 1);
    if (t <= keyframes.front().first) return keyframes.front().second;
    if (t >= keyframes.back().first)  return keyframes.back().second;

    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        float t0 = keyframes[i].first;
        float t1 = keyframes[i + 1].first;
        if (t0 <= t && t <= t1) {
            float alpha = (t - t0) / (t1 - t0);
            XMVECTOR q0 = XMLoadFloat4(&keyframes[i].second);
            XMVECTOR q1 = XMLoadFloat4(&keyframes[i + 1].second);
            XMVECTOR q = XMQuaternionSlerp(q0, q1, alpha); // slerp로 보간
            q = XMQuaternionNormalize(q); // 보간 후 정규화
            XMFLOAT4 result;
            XMStoreFloat4(&result, q);
            return result;
        }
    }
    return keyframes.back().second;
}

XMFLOAT4X4 AiToXmFloat4x4(const aiMatrix4x4& mat)
{
    XMFLOAT4X4 out;
    out._11 = mat.a1; out._12 = mat.a2; out._13 = mat.a3; out._14 = mat.a4;
    out._21 = mat.b1; out._22 = mat.b2; out._23 = mat.b3; out._24 = mat.b4;
    out._31 = mat.c1; out._32 = mat.c2; out._33 = mat.c3; out._34 = mat.c4;
    out._41 = mat.d1; out._42 = mat.d2; out._43 = mat.d3; out._44 = mat.d4;
    return out;
}
