#pragma once

#include <d3dUtil.h>

using namespace std;
using namespace DirectX;

struct Vertex 
{
    Vertex() = default;
    Vertex(const XMFLOAT3& p, const XMFLOAT3& n, const XMFLOAT2& t) :
        Pos(p), Normal(n), TexC(t) {}
    Vertex(float px, float py, float pz,
        float nx, float ny, float nz,
        float tx, float ty) :
        Pos({ px, py, pz }), Normal({ nx, ny,nz }), TexC({tx, ty}) {}

    XMFLOAT3 Pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT3 Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);;
    XMFLOAT2 TexC = XMFLOAT2(0.0f, 0.0f);;
};

struct SkinnedVertex
{
    XMFLOAT3 Pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT3 Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);;
    XMFLOAT2 TexC = XMFLOAT2(0.0f, 0.0f);;
    XMFLOAT3 BoneWeights = XMFLOAT3(0.0f, 0.0f, 0.0f);
    uint32_t BoneIndices[4] = {0, 0, 0, 0};
};

struct Submesh
{
    Submesh() = default;
    Submesh(uint32_t indexCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, BoundingBox boundingBox) :
        IndexCount(indexCount), StartIndexLocation(startIndexLocation), BaseVertexLocation(baseVertexLocation), Bounds(boundingBox) {}

    uint32_t IndexCount = 0;
    uint32_t StartIndexLocation = 0;
    uint32_t BaseVertexLocation = 0;

    BoundingBox Bounds;
};

class Mesh
{
public:
    vector<Vertex> vertices;
    vector<uint32_t> indices;

    void clear() { 
        vertices.clear();
        indices.clear();
        indices16.clear();
    }
private:
    vector<uint16_t> indices16;
};

class SkinnedMesh
{
public:
    vector<SkinnedVertex> vertices;
    vector<uint32_t> indices;

    void clear()
    {
        vertices.clear();
        indices.clear();
        indices16.clear();
    }
private:
    vector<uint16_t> indices16;
};

struct MeshGeometry
{
    // 이름을 지정해두면, 이름을 통해 lookup(검색) 가능하다.
    std::string Name;

    // 시스템 메모리에 저장된 복사본.
    // vertex/index 형식은 일반적이므로 Blob을 사용한다.
    // 실제 형 변환은 클라이언트가 적절히 처리해야 한다.
    Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

    // 버퍼에 대한 데이터
    UINT VertexByteStride = 0;
    UINT VertexBufferByteSize = 0;
    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R32_UINT;
    UINT IndexBufferByteSize = 0;

    // 하나의 MeshGeometry는 여러 geometry를 하나의 vertex/index 버퍼에 저장할 수 있다.
    // 이 컨테이너를 사용해 SubmeshGeometry를 정의하면, 개별 Submesh를 따로 렌더링할 수 있다.
    std::unordered_map<std::string, Submesh> DrawArgs;

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
        vbv.StrideInBytes = VertexByteStride;
        vbv.SizeInBytes = VertexBufferByteSize;

        return vbv;
    }

    D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
    {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
        ibv.Format = IndexFormat;
        ibv.SizeInBytes = IndexBufferByteSize;

        return ibv;
    }

    // GPU 업로드가 끝나면 이 메모리는 해제할 수 있다.
    void DisposeUploaders()
    {
        VertexBufferUploader = nullptr;
        IndexBufferUploader = nullptr;
    }
};