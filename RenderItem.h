#pragma once
#include "d3dUtil.h"
#include "Mesh.h"
#include "SkinnedData.h"


enum class RenderLayer : uint8_t
{
    Opaque = 0,
    Transparent,
    AlphaTested,
    Skinned,
    Instance,
    Count
};


// 하나의 물체를 그리는 데 필요한 매개변수들을 담는 가벼운 구조체
// 이런 구조체의 구체적인 구성은 응용 프로그램마다 다를 수 있다.
struct RenderItem
{
    RenderItem() = default;
    RenderItem(const MeshGeometry* mesh, const Submesh& submesh, const Material* material, 
        const XMFLOAT4X4& world, const XMFLOAT4X4& texTransform) :
        mesh_(mesh), material_(material), world_(world), texTransform_(texTransform)
    {
        indexCount_ = submesh.numIndices_;
        baseIndex_ = submesh.baseIndex_;
        baseVertex_ = submesh.baseVertex_;
    }
    RenderItem(const MeshGeometry* mesh, const Submesh& submesh, const Material* material,
        const XMMATRIX& world, const XMMATRIX& texTransform) :
        mesh_(mesh), material_(material)
    {
        XMStoreFloat4x4(&world_, world);
        XMStoreFloat4x4(&texTransform_, texTransform);

        indexCount_ = submesh.numIndices_;
        baseIndex_ = submesh.baseIndex_;
        baseVertex_ = submesh.baseVertex_;
    }

    void SetFrameDirty() { numFramesDirty_ = gNumFrameResources; }

    // 셰계 공간을 기준으로 물체의 국소 공간을 서술하는 세계 행렬
    // 이 행렬은 세계공간에서의 물체의 크기, 회전, 위치를 결정.
    XMFLOAT4X4 world_ = MathHelper::Identity4x4();

    XMFLOAT4X4 texTransform_ = MathHelper::Identity4x4();

    // 더티 플래그는 물체의 자료가 변해서 버퍼를 갱신해야 하는지의 여부를 나타낸다.
    // 물체의 자료를 수정할 때에는 반드시 NumFramesDirty = gNumFrameResources로 설정한다.
    //  그래야 각각의 프레임 자원이 갱신된다.
    int numFramesDirty_ = gNumFrameResources;

    // GPU 상수 버퍼의 색인
    uint32_t objCBIndex_ = -1;

    const Material* material_ = nullptr;
    const MeshGeometry* mesh_ = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY primitiveType_ = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // fristum culling
    BoundingBox boundingBox_;
    BoundingSphere boundingSphere_;

    // DrawIndexedInstanced parameters.
    uint32_t indexCount_ = 0;
    uint32_t baseIndex_ = 0;
    uint32_t baseVertex_ = 0;

    // Only applicable to skinned render-items.
    bool isSkinningObject = false;
    uint32_t skinnedCBIndex_ = -1;

    // nullptr if this render-item is not animated by skinned mesh.
    SkinnedModelInstance* skinnedModelInst_ = nullptr;

    // instance data
    uint32_t instanceCount_ = 1;
    vector<InstanceData> instances_;
    uint32_t instanceOffset_ = 0;
};
