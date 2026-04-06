#pragma once

#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"


extern const int NUM_FRAME_RESOURCES;

#define MaxLights 16

struct Light
{
    DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
    float FalloffStart = 1.0f;                          // point/spot Light
    DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot Light
    float FalloffEnd = 10.0f;                           // point/spot Light
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot Light
    float SpotPower = 64.0f;                            // spot Light
};

struct ObjectConstants
{
    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    UINT     MaterialIndex = 0;

    union
    {
        UINT ObjPad0;
        UINT IsBillboardYAxisFixed = 0; // 0: 자유 회전, 1: Y축 고정
    };
    UINT     ObjPad1;
    UINT     ObjPad2;
};

struct InstanceData
{
    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    UINT MaterialIndex = 0;
    UINT InstancePad0;
    UINT InstancePad1;
    UINT InstancePad2;
};

struct MaterialData
{
    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 0.5f;

    // Used in texture mapping.
    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

    UINT DiffuseMapIndex = 0;
    UINT MaterialPad0;
    UINT MaterialPad1;
    UINT MaterialPad2;
};

struct SkinnedConstants
{
    DirectX::XMFLOAT4X4 BoneTransforms[96];
};

struct PassConstants
{
    DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();

    DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };

    float NearZ = 0.0f;
    float FarZ = 0.0f;

    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;

    DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

    DirectX::XMFLOAT4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
    float gFogStart = 5.0f;
    float gFogRange = 150.0f;
    DirectX::XMFLOAT2 cbPerObjectPad2;

    Light Lights[MaxLights];
};

// CPU가 한 프레임의 명령 목록들을 구축하는 데 필요한 자원들을 대표하는 클래스
// 응용 프로그램마다 필요한 자원이 다를 것 이므로, 이런 클래스의 멤버 구성 역시 
// 응용 프로그램마다 달라야한다.
struct FrameResource
{
public:
    
    FrameResource(ID3D12Device* device, 
        uint32_t passCount, 
        uint32_t objectCount, 
        uint32_t skinnedObjectCount, 
        uint32_t maxInstanceCount,
        uint32_t materialCount);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource();

    // 명령 할당자는 GPU가 명령들을 다 처리한 후 재설정해야 한다.
    // 따라서 프레임마다 할당자가 필요하다.
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc_;

    // 상수 버퍼는 그것을 참조하는 명령들을 GPU가 다 처리한 후에 갱신해야한다.
    // 따라서 프레임마다 상수 버퍼를 새로 만들어야한다.
    std::unique_ptr<UploadBuffer<PassConstants>> passCB_ = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> objectCB_ = nullptr;
    std::unique_ptr<UploadBuffer<MaterialData>> materialBuffer_ = nullptr;
    std::unique_ptr<UploadBuffer<SkinnedConstants>> skinnedCB_ = nullptr;

    // 서로 다른 메시도 전부 몰아넣은 뒤 offset으로 접근
    std::unique_ptr<UploadBuffer<InstanceData>> instanceBuffer_ = nullptr;
    // Fence는 현재 울타리 지점까지의 명령들을 표시하는 값이다.
    // 이 값은 GPU가 아직 이 프레임 자원을 사용하고 있는지 판정하는 용도로 쓰인다.
    uint64_t fence_ = 0;
};