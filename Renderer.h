#pragma once
#include "d3dUtil.h"
#include "FrameResource.h"
#include "Camera.h"
#include "Scene.h"
#include "GameTimer.h"

using Microsoft::WRL::ComPtr;

class Renderer
{
public:
	Renderer();
	~Renderer();

	ID3D12Device* GetDevice();
	ID3D12GraphicsCommandList* GetCommandList();
	
	void CommandListReset();
	void CommandListClose();

	bool InitDirect3D(HWND hwnd, int clientWidth, int clientHeight);
	void InitScene(Scene* scene);
	void OnResize(int clientWidth, int clientHeight);

	void Update(const GameTimer& gt, Scene* scene);
	
	void Draw(const Scene* scene);

	void FlushCommandQueue();
private:
	bool InitDirect3D(HWND hwnd);

	void CreateCommandObjects();
	void CreateSwapChain(HWND hwnd);
	void CreateRtvAndDsvDescriptorHeaps();

	ID3D12Resource* CurrentBackBuffer()const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

	void BuildRootSignature();
	void BuildDescriptorHeaps(Scene* scene);
	void BuildShadersAndInputLayout();
	void BuildFrameResources(Scene* scene);
	void BuildPSOs();
	void BuildDebugMesh();
	void UpdateDebugMesh(Scene* scene);

	array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	void UpdateObjectCBs(const GameTimer& gt, Scene* scene);
	void UpdateInstanceData(const GameTimer& gt, Scene* scene);
	void UpdateSkinnedCBs(const GameTimer& gt, Scene* scene);
	void UpdateMaterialBuffer(const GameTimer& gt, Scene* scene);
	void UpdateMainPassCB(const GameTimer& gt, Scene* scene);

	void DrawRenderItems(const vector<RenderItem*>& ritems);
	void DrawDebugBox();

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);
private:
	// ===========================================================
	// DirectX 12 Core Objects
	// ===========================================================
	Microsoft::WRL::ComPtr<IDXGIFactory4>         dxgiFactory_;
	Microsoft::WRL::ComPtr<ID3D12Device>          d3dDevice_;
	Microsoft::WRL::ComPtr<IDXGISwapChain>        swapChain_;

	// ===========================================================
	// Command Objects
	// ===========================================================
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>        commandQueue_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    directCmdListAlloc_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

	// ===========================================================
	// Synchronization
	// ===========================================================
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	UINT64 currentFence_ = 0;

	// ===========================================================
	// Swap Chain Buffers
	// ===========================================================
	static const int numSwapChainBuffers_ = 2;
	int currBackBuffer_ = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainBuffer_[numSwapChainBuffers_];
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilBuffer_;

	// ===========================================================
	// Descriptor Heaps
	// ===========================================================
	ComPtr<ID3D12DescriptorHeap> rtvHeap_;
	ComPtr<ID3D12DescriptorHeap> dsvHeap_;
	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_ = nullptr;

	UINT rtvDescriptorSize_ = 0;
	UINT dsvDescriptorSize_ = 0;
	UINT cbvSrvDescriptorSize_ = 0;
	UINT cbvSrvUavDescriptorSize_ = 0;

	// ===========================================================
	// Pipeline / Shaders
	// ===========================================================
	ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	unordered_map<string, ComPtr<ID3DBlob>> shaders_;
	unordered_map<string, ComPtr<ID3D12PipelineState>> PSOs_;
	unordered_map<string, vector<D3D12_INPUT_ELEMENT_DESC>> inputLayouts_;

	// ===========================================================
	// Frame Resources
	// ===========================================================
	vector<unique_ptr<FrameResource>> frameResources_;
	FrameResource* currFrameResource_ = nullptr;
	int currFrameResourceIndex_ = 0;

	// ===========================================================
	// Viewport & Scissor
	// ===========================================================
	D3D12_VIEWPORT screenViewport_;
	D3D12_RECT     scissorRect_;

	// ===========================================================
	// Camera / Pass Constants
	// ===========================================================
	Camera* mainCamera_ = nullptr;
	PassConstants mainPassCB_;

	// ===========================================================
	// MSAA Settings
	// ===========================================================
	bool msaaState_ = false;  // 4X MSAA 활성화 여부
	UINT msaaQuality_ = 0;      // 4X MSAA 품질 레벨

	// ===========================================================
	// SwapChain / Depth Format
	// ===========================================================
	D3D_DRIVER_TYPE d3dDriverType_ = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT     backBufferFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT     depthStencilFormat_ = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// ===========================================================
	// Client Info
	// ===========================================================
	int clientWidth_;
	int clientHeight_;

	// ===========================================================
	// Debug Mesh
	// ===========================================================
	MeshGeometry debugMesh_;
	uint32_t countDebugVertices_;
	const uint32_t maxDebugVertices_ = 10000;
	UINT8* mappedData_ = nullptr;
};

