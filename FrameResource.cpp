#include "FrameResource.h"

FrameResource::FrameResource(
    ID3D12Device* device,
    uint32_t passCount,
    uint32_t objectCount,
    uint32_t skinnedObjectCount,
    uint32_t maxInstanceCount,
    uint32_t materialCount)
{
    ThrowIfFailed(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(cmdListAlloc_.GetAddressOf())));

    passCB_ = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
    materialBuffer_ = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, false);
    instanceBuffer_ = std::make_unique<UploadBuffer<InstanceData>>(device, maxInstanceCount, false);
    objectCB_ = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
    skinnedCB_ = std::make_unique<UploadBuffer<SkinnedConstants>>(device, skinnedObjectCount, true);
}

FrameResource::~FrameResource()
{

}