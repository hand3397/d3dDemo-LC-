#define CGLTF_IMPLEMENTATION
#include "ModelLoader.h"

bool ModelLoader::ReadModel(const char* filename)
{
    this->Clear();

    cgltf_options options = {};
    cgltf_data* data = nullptr;

    
    cgltf_result result = cgltf_parse_file(&options, filename, &data);
    if (result != cgltf_result_success) {
        //std::cerr << "Failed to parse glTF\n";
        return false;
    }
    
    result = cgltf_load_buffers(&options, data, filename);
    if (result != cgltf_result_success) {
        //std::cerr << "Failed to load buffers\n";`
        cgltf_free(data);
        return false;
    }

    ReadSkinnedMesh(data);
    
    cgltf_free(data);

    return true;
}

void ModelLoader::ReadSkinnedMesh(cgltf_data* data)
{
    int numMeshes = data->meshes_count;
    size_t baseVertex = 0;
    size_t baseIndex = 0;
    size_t numIndices = 0;

    for (size_t mi = 0; mi < data->meshes_count; ++mi) {
        cgltf_mesh* mesh = &data->meshes[mi];

        for (size_t pi = 0; pi < mesh->primitives_count; ++pi) {
            cgltf_primitive* primitive = &mesh->primitives[pi];

            // POSITION 접근
            cgltf_accessor* posAccessor = nullptr;
            cgltf_accessor* normalAccessor = nullptr;
            cgltf_accessor* texAccessor = nullptr;
            //cgltf_accessor* tangentAccessor = nullptr;
            cgltf_accessor* indicesAccessor = nullptr;
            cgltf_accessor* jointsAccessor = nullptr;
            cgltf_accessor* weightsAccessor = nullptr;

            for (size_t ai = 0; ai < primitive->attributes_count; ++ai) {
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

            for (size_t i = 0; i < posAccessor->count; ++i) {
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

void ModelLoader::Clear()
{
    mesh.clear();
    skinnedMesh.clear();
}