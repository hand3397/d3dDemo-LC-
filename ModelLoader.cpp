#include "ModelLoader.h"

bool ModelLoader::ReadModel(const char* filename)
{
    this->Clear();

    cgltf_options options = {};
    cgltf_data* data = nullptr;

    const char* filename = "model.glb";
    cgltf_result result = cgltf_parse_file(&options, filename, &data);
    if (result != cgltf_result_success) {
        //std::cerr << "Failed to parse glTF\n";
        return false;
    }

    result = cgltf_load_buffers(&options, data, filename);
    if (result != cgltf_result_success) {
        //std::cerr << "Failed to load buffers\n";
        cgltf_free(data);
        return false;
    }



    cgltf_free(data);

    return false;
}

void ModelLoader::ReadSkinedMesh(cgltf_data* data)
{
    data->accessors->buffer_view
}

void ModelLoader::Clear()
{
    mesh.clear();
    skinedMesh.clear();
}