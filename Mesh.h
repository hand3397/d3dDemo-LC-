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
    XMFLOAT3 Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT2 TexC = XMFLOAT2(0.0f, 0.0f);
};

struct ColorVertex
{
    ColorVertex() = default;
    ColorVertex(const XMFLOAT3& p, const XMFLOAT4& c) :
        Pos(p), Color(c) {}
    ColorVertex(const XMFLOAT3& p, const XMVECTOR& c) :
        Pos(p) {
        XMStoreFloat4(&Color, c);
    }

    XMFLOAT3 Pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT4 Color = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
};

struct SkinnedVertex
{
    XMFLOAT3 Pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT3 Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT2 TexC = XMFLOAT2(0.0f, 0.0f);
    XMFLOAT3 BoneWeights = XMFLOAT3(0.0f, 0.0f, 0.0f);
    uint32_t BoneIndices[4] = {0, 0, 0, 0};
};

struct BillboardVertex
{
    XMFLOAT3 Pos = XMFLOAT3(0.0f, 0.0f, 0.0f); // Center of the billboard
    XMFLOAT2 Size = XMFLOAT2(0.0f, 0.0f);
};

struct Submesh
{
    Submesh() = default;
    Submesh(uint32_t indexCount, uint32_t baseIndex, uint32_t baseVertex, 
        const BoundingBox& boundingBox, const BoundingSphere& boundingSphere) :
        numIndices_(indexCount), baseIndex_(baseIndex), baseVertex_(baseVertex), 
        boundingBox_(boundingBox), boundingSphere_(boundingSphere){}

    uint32_t numIndices_ = 0;
    uint32_t baseIndex_ = 0;
    uint32_t baseVertex_ = 0;

    BoundingBox boundingBox_;
    BoundingSphere boundingSphere_;
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
    std::string name_;

    // 시스템 메모리에 저장된 복사본.
    // vertex/index 형식은 일반적이므로 Blob을 사용한다.
    // 실제 형 변환은 클라이언트가 적절히 처리해야 한다.
    Microsoft::WRL::ComPtr<ID3DBlob> vertexBufferCPU_ = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> indexBufferCPU_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferGPU_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferGPU_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferUploader_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferUploader_ = nullptr;

    // 버퍼에 대한 데이터
    UINT vertexByteStride_ = 0;
    UINT vertexBufferByteSize_ = 0;
    DXGI_FORMAT indexFormat_ = DXGI_FORMAT_R32_UINT;
    UINT indexBufferByteSize_ = 0;

    // 하나의 MeshGeometry는 여러 geometry를 하나의 vertex/index 버퍼에 저장할 수 있다.
    // 이 컨테이너를 사용해 SubmeshGeometry를 정의하면, 개별 Submesh를 따로 렌더링할 수 있다.
    std::unordered_map<std::string, Submesh> subMeshes_;

    template<typename VertexType, typename IndexType>
    void BuildMeshGeo(const string& name, const vector<VertexType>& vertices, const vector<IndexType>& indices,
        ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* cmdList)
    {
        name_ = name;

        const uint32_t vbByteSize = (uint32_t)vertices.size() * sizeof(VertexType);
        const uint32_t ibByteSize = (uint32_t)indices.size() * sizeof(IndexType);

        ThrowIfFailed(D3DCreateBlob(vbByteSize, &vertexBufferCPU_));
        CopyMemory(vertexBufferCPU_->GetBufferPointer(), vertices.data(), vbByteSize);

        ThrowIfFailed(D3DCreateBlob(ibByteSize, &indexBufferCPU_));
        CopyMemory(indexBufferCPU_->GetBufferPointer(), indices.data(), ibByteSize);

        vertexBufferGPU_ = d3dUtil::CreateDefaultBuffer(d3dDevice,
            cmdList, vertices.data(), vbByteSize, vertexBufferUploader_);

        indexBufferGPU_ = d3dUtil::CreateDefaultBuffer(d3dDevice,
            cmdList, indices.data(), ibByteSize, indexBufferUploader_);

        vertexByteStride_ = sizeof(VertexType);
        vertexBufferByteSize_ = vbByteSize;
        indexFormat_ = (is_same<IndexType, uint16_t>::value) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
        indexBufferByteSize_ = ibByteSize;
    }

    void AddSubmesh(const string& name, const Submesh& submesh)
    {
        subMeshes_[name] = submesh;
    }

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = vertexBufferGPU_->GetGPUVirtualAddress();
        vbv.StrideInBytes = vertexByteStride_;
        vbv.SizeInBytes = vertexBufferByteSize_;

        return vbv;
    }

    D3D12_VERTEX_BUFFER_VIEW VertexUploadBufferView()const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = vertexBufferUploader_->GetGPUVirtualAddress();
        vbv.StrideInBytes = vertexByteStride_;
        vbv.SizeInBytes = vertexBufferByteSize_;

        return vbv;
    }

    D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
    {
        D3D12_INDEX_BUFFER_VIEW ibv = {};
        ibv.BufferLocation = indexBufferGPU_->GetGPUVirtualAddress();
        ibv.Format = indexFormat_;
        ibv.SizeInBytes = indexBufferByteSize_;

        return ibv;
    }

    // GPU 업로드가 끝나면 이 메모리는 해제할 수 있다.
    void DisposeUploaders()
    {
        vertexBufferUploader_ = nullptr;
        indexBufferUploader_ = nullptr;
    }
};