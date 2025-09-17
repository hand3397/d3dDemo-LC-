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

ModelLoader::ModelLoader()
{
    importer_.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
}

bool ModelLoader::ReadModelFile(const char* fileName, float globalScale = 1.0f)
{
    this->Clear();

    /*
        aiProcess_JoinIdenticalVertices |        // 동일한 꼭지점 결합, 인덱싱 최적화
        aiProcess_ValidateDataStructure |        // 로더의 출력을 검증
        aiProcess_ImproveCacheLocality |        // 출력 정점의 캐쉬위치를 개선
        aiProcess_RemoveRedundantMaterials |    // 중복된 매터리얼 제거
        aiProcess_GenUVCoords |                    // 구형, 원통형, 상자 및 평면 매핑을 적절한 UV로 변환
        aiProcess_TransformUVCoords |            // UV 변환 처리기 (스케일링, 변환...)
        aiProcess_FindInstances |                // 인스턴스된 매쉬를 검색하여 하나의 마스터에 대한 참조로 제거
        aiProcess_LimitBoneWeights |            // 정점당 뼈의 가중치를 최대 4개로 제한
        aiProcess_OptimizeMeshes |                // 가능한 경우 작은 매쉬를 조인
        aiProcess_GenSmoothNormals |            // 부드러운 노말벡터(법선벡터) 생성
        aiProcess_SplitLargeMeshes |            // 거대한 하나의 매쉬를 하위매쉬들로 분활(나눔)
        aiProcess_Triangulate |                    // 3개 이상의 모서리를 가진 다각형 면을 삼각형으로 나눔
        aiProcess_ConvertToLeftHanded |            // D3D의 왼손좌표계로 변환
    */

    importer_.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, globalScale);

    // 모델 파일 로드 (예: soldier.fbx, model.gltf, model.obj 등)
    const aiScene* scene = importer_.ReadFile(fileName,
        aiProcess_Triangulate |        // 삼각형으로 변환
        aiProcess_CalcTangentSpace |   // Normal / Tangent 계산
        aiProcess_JoinIdenticalVertices | 
        aiProcess_LimitBoneWeights |
        aiProcess_ConvertToLeftHanded |
        aiProcess_GlobalScale
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
        subMeshes_.push_back({ numIndices , baseIndex , baseVertex , boundingBox });
        baseVertex += mesh->mNumVertices;
        baseIndex += numIndices;
    }
}

void ModelLoader::ReadNodeData(const aiScene* scene)
{
    unordered_map<string, uint32_t> nodeToIdx;

    queue<aiNode*> q;
    q.push(scene->mRootNode);

    while (!q.empty()) {
        aiNode* node = q.front(); q.pop();
        nodeToIdx.insert({ node->mName.C_Str(), nodeToIdx.size() });

        for (uint32_t ci = 0; ci < node->mNumChildren; ci++)
            q.push(node->mChildren[ci]);
    }

    vector<uint32_t> parentNode(nodeToIdx.size());
    vector<vector<uint32_t>> childrenNode(nodeToIdx.size());
    vector<XMFLOAT4X4> nodeTransforms(nodeToIdx.size());

    q.push(scene->mRootNode);
    while (!q.empty()) {
        aiNode* node = q.front(); q.pop();
        uint32_t nodeIdx = nodeToIdx[node->mName.C_Str()];

        if (node->mParent != nullptr) {
            parentNode[nodeIdx] = nodeToIdx[node->mParent->mName.C_Str()];
        }            
        else {
            parentNode[nodeIdx] = nodeIdx;
        }

        XMFLOAT4X4 nodeTransform;
        XMStoreFloat4x4(&nodeTransform, XMMatrixTranspose(XMMATRIX(&node->mTransformation.a1)));
        nodeTransforms[nodeIdx] = nodeTransform;

        for (uint32_t ci = 0; ci < node->mNumChildren; ci++) {
            childrenNode[nodeIdx].push_back(nodeToIdx[node->mChildren[ci]->mName.C_Str()]);
            q.push(node->mChildren[ci]);
        }
    }

    skinnedData_.SetNode(parentNode, childrenNode, nodeToIdx, nodeTransforms);
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

        uint32_t baseVertex = subMeshes_[mi].BaseVertexLocation;

        for (uint32_t bi = 0; bi < numBones; bi++) {
            aiBone* bone = mesh->mBones[bi];
            int32_t nodeIdx = skinnedData_.NodeToIdx(bone->mName.C_Str());
            // 노드에 저장되지 않은 bone은 무시
            if (nodeIdx == -1)
                continue;

            if (nodeToBone.find(nodeIdx) == nodeToBone.end()) {
                nodeToBone.insert({ nodeIdx, nodeToBone.size() });
                XMFLOAT4X4 offsetMat;
                XMStoreFloat4x4(&offsetMat, XMMatrixTranspose(XMMATRIX(&bone->mOffsetMatrix.a1)));
                boneOffsets.push_back(offsetMat);
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

bool ModelLoader::ReadAnimationFile(const char* fileName, const string& animationName)
{
    // 모델 파일 로드 (예: soldier.fbx, model.gltf, model.obj 등)
    const aiScene* scene = importer_.ReadFile(fileName,
        aiProcess_Triangulate |        // 삼각형으로 변환
        aiProcess_CalcTangentSpace |   // Normal / Tangent 계산
        aiProcess_JoinIdenticalVertices |
        aiProcess_LimitBoneWeights |
        aiProcess_ConvertToLeftHanded |
        aiProcess_GlobalScale
    );

    if (!scene) {
        //std::cerr << "Assimp Error: " << importer.GetErrorString() << std::endl;
        return false;
    }

    if (scene->HasAnimations()) {
        ReadAnimations(scene, animationName);
        return true;
    }
    return false;
}

void ModelLoader::ReadAnimations(const aiScene* scene, const string& animationName)
{
    uint32_t numAnimations = scene->mNumAnimations;
    for (uint32_t ai = 0; ai < numAnimations; ai++) {
        aiAnimation* anim = scene->mAnimations[ai];
        
        string animName = animationName + to_string(ai);

        AnimationClip animationClip;
        animationClip.SetDuration(anim->mDuration, anim->mTicksPerSecond);
        
        animationClip.boneAnimations_.resize(skinnedData_.NodeCount());

        uint32_t numChannels = anim->mNumChannels;
        for (uint32_t ci = 0; ci < numChannels; ci++) {
            aiNodeAnim* channel = anim->mChannels[ci];

            string nodeName = channel->mNodeName.C_Str();
            uint32_t nodeIdx = skinnedData_.NodeToIdx(nodeName);
            if (nodeIdx == -1)
                continue;

            vector<pair<double, XMFLOAT3>> positions(channel->mNumPositionKeys);
            for (uint32_t pi = 0; pi < channel->mNumPositionKeys; pi++) {
                const aiVectorKey &key = channel->mPositionKeys[pi];
                positions[pi] = { key.mTime, {key.mValue.x, key.mValue.y, key.mValue.z} };
            }

            vector<pair<double, XMFLOAT4>> rotateQuats(channel->mNumRotationKeys);
            for (uint32_t pi = 0; pi < channel->mNumRotationKeys; pi++) {
                const aiQuatKey& key = channel->mRotationKeys[pi];
                rotateQuats[pi] = { key.mTime, {key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w} };
            }

            vector<pair<double, XMFLOAT3>> scales(channel->mNumScalingKeys);
            for (uint32_t pi = 0; pi < channel->mNumScalingKeys; pi++) {
                const aiVectorKey& key = channel->mScalingKeys[pi];
                scales[pi] = { key.mTime, {key.mValue.x, key.mValue.y, key.mValue.z} };
            }
            
            InterpolateKeyframes(animationClip.boneAnimations_[nodeIdx].keyframes_, 
                positions, rotateQuats, scales);
        }

        animationClip.SetClipTime();
        skinnedData_.AddAnimaiton(animName, animationClip);
    }
}

void ModelLoader::InterpolateKeyframes(vector<Keyframe>& keyframes, 
    const vector<pair<double, XMFLOAT3>>& positions,
    const vector<pair<double, XMFLOAT4>>& rotateQuats,
    const vector<pair<double, XMFLOAT3>>& scales)
{
    vector<double> times;
    times.reserve(positions.size() + rotateQuats.size() + scales.size());

    for (auto& kv : positions)  
        times.push_back(kv.first);
    for (auto& kv : rotateQuats) 
        times.push_back(kv.first);
    for (auto& kv : scales)    
        times.push_back(kv.first);

    sort(times.begin(), times.end());

    auto it = std::unique(times.begin(), times.end(), MathHelper::FloatEqual());
    times.erase(it, times.end());

    uint32_t numTimes = times.size();
    keyframes.resize(numTimes);

    // time마다 보간된 keyframe 구하기
    for (uint32_t i = 0; i < numTimes; ++i) {
        double t = times[i];
        XMFLOAT3 p = SampleFloat3(positions, t);
        XMFLOAT4 r = SampleFloat4(rotateQuats, t);
        XMFLOAT3 s = SampleFloat3(scales, t);

        Keyframe kf;
        kf.timePos_ = t;
        kf.scale_ = s;
        kf.rotationQuat_ = r;
        kf.translation_ = p;

        keyframes[i] = kf;
    }
}

void ModelLoader::Clear()
{
    mesh_.clear();
    skinnedMesh_.clear();
    skinnedData_ = SkinnedData();
    subMeshes_.clear();
}

XMFLOAT3 SampleFloat3(const vector<pair<double, XMFLOAT3>>& keyframes, double t)
{
    if (keyframes.empty()) return XMFLOAT3(1, 1, 1);
    if (t <= keyframes.front().first) return keyframes.front().second;
    if (t >= keyframes.back().first)  return keyframes.back().second;

    // t를 감싸는 두 키프레임 찾기
    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        double t0 = keyframes[i].first;
        double t1 = keyframes[i + 1].first;
        if (t0 <= t && t <= t1) {
            double alpha = (t - t0) / (t1 - t0);
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

XMFLOAT4 SampleFloat4(const vector<pair<double, XMFLOAT4>>& keyframes, double t)
{
    if (keyframes.empty()) return XMFLOAT4(0, 0, 0, 1);
    if (t <= keyframes.front().first) return keyframes.front().second;
    if (t >= keyframes.back().first)  return keyframes.back().second;

    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        double t0 = keyframes[i].first;
        double t1 = keyframes[i + 1].first;
        if (t0 <= t && t <= t1) {
            double alpha = (t - t0) / (t1 - t0);
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
