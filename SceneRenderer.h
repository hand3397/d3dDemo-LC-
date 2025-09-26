#pragma once
#include "Scene.h"
#include "FrameResource.h"

class SceneRenderer
{
public:
    SceneRenderer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    ~SceneRenderer();

    void InitializeDirect3D(ID3D12Device* device,
        UINT numRTVs, UINT numDSVs);
    void InitializeScene(Scene* scene);
    void OnResize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
        bool msaaState, UINT msaaQuality, UINT width, UINT height);
    void Draw(const GameTimer& gt, Scene* scene, Camera* camera);

private:
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView(int currentBackBuffer)const;
    void CreateRtvAndDsvDescriptorHeaps(ID3D12Device* device, UINT numRTVs, UINT numDSVs);


    void BuildRootSignature();
    void BuildDescriptorHeaps(Scene* scene);
    void BuildShadersAndInputLayout();
    void BuildPSOs();
    void BuildFrameResources();

    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& ritems);

    array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
    ID3D12Device* device_ = nullptr;
    ID3D12GraphicsCommandList* cmdList_ = nullptr;

    UINT cbvSrvDescriptorSize_ = 0;

    ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_ = nullptr;

    unordered_map<string, ComPtr<ID3DBlob>> shaders_;
    unordered_map<string, ComPtr<ID3D12PipelineState>> PSOs_;

    vector<D3D12_INPUT_ELEMENT_DESC> inputLayout_;
    vector<D3D12_INPUT_ELEMENT_DESC> skinnedInputLayout_;

    vector<unique_ptr<FrameResource>> frameResources_;
    FrameResource* currFrameResource_ = nullptr;
    int currFrameResourceIndex_ = 0;

    PassConstants mainPassCB_;

    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilBuffer_;

    D3D12_VIEWPORT screenViewport_;
    D3D12_RECT scissorRect_;

    DXGI_FORMAT depthStencilFormat_ = DXGI_FORMAT_D24_UNORM_S8_UINT;
};

