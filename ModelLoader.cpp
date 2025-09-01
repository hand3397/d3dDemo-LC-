#define CGLTF_IMPLEMENTATION
#include "ModelLoader.h"

bool ModelLoader::ReadModel(const char* fileName)
{
    this->Clear();

    cgltf_options options = {};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, fileName, &data);
    if (result != cgltf_result_success) {
        //std::cerr << "Failed to parse glTF\n";
        return false;
    }
    
    result = cgltf_load_buffers(&options, data, fileName);
    if (result != cgltf_result_success) {
        //std::cerr << "Failed to load buffers\n";`
        cgltf_free(data);
        return false;
    }

    ReadSkinnedMesh(data);
    ReadSkinnedData(data);

    cgltf_free(data);

    return true;
}

void ModelLoader::ReadSkinnedMesh(const cgltf_data* data)
{
    int numMeshes = data->meshes_count;
    size_t baseVertex = 0;
    size_t baseIndex = 0;
    size_t numIndices = 0;

    for (int mi = 0; mi < data->meshes_count; ++mi) {
        cgltf_mesh* mesh = &data->meshes[mi];

        for (int pi = 0; pi < mesh->primitives_count; ++pi) {
            cgltf_primitive* primitive = &mesh->primitives[pi];

            // POSITION 접근
            cgltf_accessor* posAccessor = nullptr;
            cgltf_accessor* normalAccessor = nullptr;
            cgltf_accessor* texAccessor = nullptr;
            //cgltf_accessor* tangentAccessor = nullptr;
            cgltf_accessor* indicesAccessor = nullptr;
            cgltf_accessor* jointsAccessor = nullptr;
            cgltf_accessor* weightsAccessor = nullptr;

            for (int ai = 0; ai < primitive->attributes_count; ++ai) {
                cgltf_attribute* attr = &primitive->attributes[ai];
                switch (attr->type) {
                case cgltf_attribute_type_position: posAccessor = attr->data; break;
                case cgltf_attribute_type_normal:   normalAccessor = attr->data; break;
                case cgltf_attribute_type_texcoord: texAccessor = attr->data; break;
                //case cgltf_attribute_type_tangent:  tangentAccessor = attr->data; break;
                case cgltf_attribute_type_joints:   jointsAccessor = attr->data; break;
                case cgltf_attribute_type_weights:  weightsAccessor = attr->data; break;
                default: break;
                }
            }

            if (!posAccessor) 
                continue;

            BoundingBox boundingBox;
            Submesh submesh;
            submesh.BaseVertexLocation = baseVertex;
            submesh.StartIndexLocation = baseIndex;
            
            baseVertex += posAccessor->count;
            XMFLOAT3 maxVertex(0.0f, 0.0f, 0.0f), minVertex(0.0f, 0.0f, 0.0f);
            if (posAccessor->has_max)
                maxVertex = XMFLOAT3(posAccessor->max[0], posAccessor->max[1], posAccessor->max[2]);
            if (posAccessor->has_min)
                minVertex = XMFLOAT3(posAccessor->min[0], posAccessor->min[1], posAccessor->min[2]);

            boundingBox.Center = XMFLOAT3(maxVertex.x + minVertex.x, 
                maxVertex.y + minVertex.y, maxVertex.z + minVertex.z);
            boundingBox.Extents = XMFLOAT3(maxVertex.x - boundingBox.Center.x, 
                maxVertex.y - boundingBox.Center.y, maxVertex.z - boundingBox.Center.z);
            submesh.Bounds = boundingBox;

            for (int i = 0; i < posAccessor->count; ++i) {
                SkinnedVertex v;
                float temp[4] = {};
                unsigned int indexTemp[4] = {};

                cgltf_accessor_read_float(posAccessor, i, temp, 3);
                v.Pos = XMFLOAT3(temp[0], temp[1], temp[2]);

                if (normalAccessor) {
                    cgltf_accessor_read_float(normalAccessor, i, temp, 3);
                    v.Normal = XMFLOAT3(temp[0], temp[1], temp[2]); 
                }
                if (texAccessor) {
                    cgltf_accessor_read_float(texAccessor, i, temp, 2);
                    v.TexC = XMFLOAT2(temp[0], temp[1]); }
                if (jointsAccessor) { 
                    cgltf_accessor_read_uint(jointsAccessor, i, indexTemp, 4);
                    for (int bi = 0; bi < 4; bi++) v.BoneIndices[bi] = indexTemp[bi];
                }
                if (weightsAccessor) { 
                    cgltf_accessor_read_float(weightsAccessor, i, temp, 4);
                    v.BoneWeights = XMFLOAT3();
                }
                /*
                if (tangentAccessor) {
                    cgltf_accessor_read_float(tangentAccessor, i, temp, 4);
                    v. = XMFLOAT4(temp[0], temp[1], temp[2], temp[3]);
                }
                */
                skinnedMesh.vertices.push_back(v);
            }

            // 인덱스 읽기
            if (primitive->indices) {
                cgltf_accessor* indexAccessor = primitive->indices;
                baseIndex += indexAccessor->count;
                numIndices = indexAccessor->count;
                for (size_t i = 0; i < indexAccessor->count; ++i) {
                    uint32_t idx = 0;
                    cgltf_accessor_read_uint(indexAccessor, i, &idx, 1);
                    skinnedMesh.indices.push_back(idx);
                }
            }
            else // indices 없는 경우 순서대로
            {
                baseIndex += posAccessor->count;
                numIndices = posAccessor->count;
                for (size_t i = 0; i < posAccessor->count; ++i)
                    skinnedMesh.indices.push_back((uint32_t)i);
            }

            submesh.IndexCount = numIndices;
            submeshes.push_back(submesh);
        }
    }
}

void ModelLoader::ReadSkinnedData(const cgltf_data* data)
{
    vector<int> parentBone;
    vector<vector<int>> childrenBone;
    unordered_map<string, uint32_t> nameToIdx;
    vector<XMFLOAT4X4> boneOffsets;

    int numSkins = data->skins_count;
    for (int si = 0; si < numSkins; si++) {
        cgltf_skin* skin = &data->skins[si];

        int numJoints = skin->joints_count;

        parentBone.resize(numJoints);
        childrenBone.resize(numJoints);
        boneOffsets.resize(numJoints);

        // 본 이름을 idx로 매핑
        for (int ji = 0; ji < numJoints; ji++) {
            cgltf_node* joint = skin->joints[ji];
            string boneName = joint->name;

            if (nameToIdx.find(boneName) == nameToIdx.end())
                nameToIdx[boneName] = nameToIdx.size();
        }

        // read bone heritage
        for (int ji = 0; ji < numJoints; ji++) {
            cgltf_node* bone = skin->joints[ji];
            string boneName = bone->name;

            if (nameToIdx.find(boneName) == nameToIdx.end())
                continue;

            int boneIdx = nameToIdx[boneName];

            string parent = bone->parent->name;
            if (nameToIdx.find(parent) == nameToIdx.end())
                parentBone[boneIdx] = boneIdx;
            else
                parentBone[boneIdx] = nameToIdx[parent];

            int numChildren = bone->children_count;
            for (int ci = 0; ci < numChildren; ci++) {
                string childName = bone->children[ci]->name;
                if (nameToIdx.find(childName) == nameToIdx.end())
                    continue;
                childrenBone[boneIdx].emplace_back(nameToIdx[childName]);
            }
        }

        // bind boneOffsets
        if (skin->inverse_bind_matrices) {
            cgltf_accessor* accessor = skin->inverse_bind_matrices;

            for (cgltf_size i = 0; i < numJoints; ++i) {
                float mat[16];
                cgltf_accessor_read_float(accessor, i, mat, 16);

                DirectX::XMMATRIX xmMat = DirectX::XMLoadFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(mat));
                xmMat = DirectX::XMMatrixTranspose(xmMat);
                DirectX::XMStoreFloat4x4(&boneOffsets[i], xmMat);
            }
        }
        else {
            XMFLOAT4X4 identity =  MathHelper::Identity4x4();
            fill(boneOffsets.begin(), boneOffsets.end(), identity);
        }

        skinnedData.Set(parentBone, childrenBone, nameToIdx, boneOffsets);
    }
}

bool ModelLoader::ReadAnimation(const char* fileName, string animationName)
{
    cgltf_options options = {};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, fileName, &data);
    if (result != cgltf_result_success) {
        //std::cerr << "Failed to parse glTF\n";
        return false;
    }

    result = cgltf_load_buffers(&options, data, fileName);
    if (result != cgltf_result_success) {
        //std::cerr << "Failed to load buffers\n";`
        cgltf_free(data);
        return false;
    }
    ReadAnimationClip(data, animationName);

    cgltf_free(data);

    return true;
}

void ModelLoader::ReadAnimationClip(const cgltf_data* data, string& animationName)
{
    if (data->animations_count == 0)
        return;

    const cgltf_animation& anim = data->animations[0];
    if (animationName.empty())
        (animationName = anim.name ? anim.name : "noName");

    int boneCount = skinnedData.BoneCount();
    vector<vector<pair<float, XMFLOAT3>>> scaleAnimation;
    scaleAnimation.resize(boneCount);
    vector<vector<pair<float, XMFLOAT4>>> rotateAnimation;
    rotateAnimation.resize(boneCount);
    vector<vector<pair<float, XMFLOAT3>>> posAnimation;
    posAnimation.resize(boneCount);

    int numChannels = anim.channels_count;
    for (cgltf_size c = 0; c < numChannels; ++c) {
        const cgltf_animation_channel& channel = anim.channels[c];

        cgltf_node* target = channel.target_node;

        if (!(channel.target_path == cgltf_animation_path_type_translation
            || channel.target_path == cgltf_animation_path_type_rotation
            || channel.target_path == cgltf_animation_path_type_scale))
            continue;

        int64_t boneIdx = skinnedData.NameToIdx(target->name);
        if (boneIdx == -1)
            continue;

        const cgltf_animation_sampler* sampler = channel.sampler;

        cgltf_accessor* input = sampler->input;
        cgltf_accessor* output = sampler->output;

        std::vector<float> times(input->count);
        cgltf_accessor_unpack_floats(input, times.data(), times.size());

        // 출력 값 (vec3 또는 quat)
        size_t elem_size = cgltf_num_components(output->type);
        std::vector<float> values(output->count * elem_size);
        cgltf_accessor_unpack_floats(output, values.data(), values.size());

        switch (channel.target_path) {
        case cgltf_animation_path_type_translation:
            posAnimation[boneIdx].resize(input->count);
            for (size_t k = 0; k < input->count; ++k) {
                posAnimation[boneIdx][k] = { times[k],
                    XMFLOAT3(values[k * elem_size], values[k * elem_size + 1],
                        values[k * elem_size + 2]) };
            }
            break;
        case cgltf_animation_path_type_rotation:
            rotateAnimation[boneIdx].resize(input->count);
            for (size_t k = 0; k < input->count; ++k) {
                rotateAnimation[boneIdx][k] = { times[k],
                    XMFLOAT4(values[k * elem_size], values[k * elem_size + 1],
                        values[k * elem_size + 2], values[k * elem_size + 3]) };
            }
            break;
        case cgltf_animation_path_type_scale:
            scaleAnimation[boneIdx].resize(input->count);
            for (size_t k = 0; k < input->count; ++k) {
                scaleAnimation[boneIdx][k] = { times[k],
                    XMFLOAT3(values[k * elem_size], values[k * elem_size + 1],
                        values[k * elem_size + 2]) };
            }
            break;
        }
    }

    skinnedData.AddAnimaiton(animationName, InterpolateAnimaitonClip(
        scaleAnimation, rotateAnimation, posAnimation));
}

AnimationClip ModelLoader::InterpolateAnimaitonClip(const vector<vector<pair<float, XMFLOAT3>>>& scaleAnimation,
    const vector<vector<pair<float, XMFLOAT4>>>& rotateAnimation, 
    const vector<vector<pair<float, XMFLOAT3>>>& posAnimation)
{
    AnimationClip animationClip;

    int boneCount = skinnedData.BoneCount();
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

        // 2. 정렬
        std::sort(times.begin(), times.end());

        // 3. 중복 제거 (EPS 허용)
        auto it = std::unique(times.begin(), times.end(), MathHelper::FloatEqual());
        times.erase(it, times.end());

        // time마다 보간된 keyframe 구하기
        for (float t : times) {
            XMFLOAT3 s = SampleFloat3(scaleAnim, t);
            XMFLOAT4 r = SampleFloat4(rotateAnim, t); // 쿼터니언
            XMFLOAT3 p = SampleFloat3(posAnim, t);

            Keyframe kf;
            kf.timePos_ = t;
            kf.scale_ = s;
            kf.rotationQuat_ = r;
            kf.translation_ = p;

            animationClip.boneAnimations_[bi].keyframes_.push_back(kf);
        }
    }

    animationClip.SetClipStartTime();
    animationClip.SetClipEndTime();

    return animationClip;
}

void ModelLoader::Clear()
{
    mesh.clear();
    skinnedMesh.clear();
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
