#pragma once

#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"


extern const int NUM_FRAME_RESOURCES;

#define MaxLights 16

struct Light
{
    DirectX::XMFLOAT3 strength_ = { 0.5f, 0.5f, 0.5f };
    float falloffStart_ = 1.0f;                          // point/spot Light
    DirectX::XMFLOAT3 direction_ = { 0.0f, -1.0f, 0.0f };// directional/spot Light
    float falloffEnd_ = 10.0f;                           // point/spot Light
    DirectX::XMFLOAT3 position_ = { 0.0f, 0.0f, 0.0f };  // point/spot Light
    float spotPower_ = 64.0f;                            // spot Light
};

struct ObjectConstants
{
    DirectX::XMFLOAT4X4 world_ = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 texTransform_ = MathHelper::Identity4x4();
    UINT     materialIndex_ = 0;
    UINT     atlasIndex_ = 0;
    union
    {
        UINT objPad0_;
        UINT isBillboardYAxisFixed_ = 0; // 0: 자유 회전, 1: Y축 고정
    };
    
    UINT     objPad1_;
};

struct InstanceData
{
    DirectX::XMFLOAT4X4 world_ = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 texTransform_ = MathHelper::Identity4x4();
    UINT materialIndex_ = 0;
    UINT instancePad0_;
    UINT instancePad1_;
    UINT instancePad2_;
};

struct MaterialData
{
    DirectX::XMFLOAT4 diffuseAlbedo_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 fresnelR0_ = { 0.01f, 0.01f, 0.01f };
    float roughness_ = 0.5f;

    // Used in texture mapping.
    DirectX::XMFLOAT4X4 matTransform_ = MathHelper::Identity4x4();
    UINT diffuseMapIndex_ = 0;
    UINT atlasWidth_;
    UINT atlasHeight_;
    UINT materialPad0_;
};

struct SkinnedConstants
{
    DirectX::XMFLOAT4X4 boneTransforms_[96];
};

struct PassConstants
{
    DirectX::XMFLOAT4X4 view_ = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 invView_ = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 proj_ = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 invProj_ = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 viewProj_ = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 invViewProj_ = MathHelper::Identity4x4();

    DirectX::XMFLOAT3 eyePosW_ = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1_ = 0.0f;
    DirectX::XMFLOAT2 renderTargetSize_ = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 invRenderTargetSize_ = { 0.0f, 0.0f };

    float nearZ_ = 0.0f;
    float farZ_ = 0.0f;

    float totalTime_ = 0.0f;
    float deltaTime_ = 0.0f;

    DirectX::XMFLOAT4 ambientLight_ = { 0.0f, 0.0f, 0.0f, 1.0f };

    DirectX::XMFLOAT4 fogColor_ = { 0.7f, 0.7f, 0.7f, 1.0f };
    float fogStart_ = 5.0f;
    float fogRange_ = 150.0f;
    DirectX::XMFLOAT2 cbPerObjectPad2_;

    Light lights_[MaxLights];
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