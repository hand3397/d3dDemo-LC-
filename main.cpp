#include "stdafx.h"
#include "d3dApp.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"
#include "Camera.h"
#include "ModelLoader.h"
#include "RenderItem.h"
#include "Player.h"
#include "Scene.h"
#include "SceneRenderer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;


const int gNumFrameResources = 3;


class Direct3DDemo : public D3DApp
{
public:
	Direct3DDemo(HINSTANCE hInstance);
	Direct3DDemo(const Direct3DDemo& rhs) = delete;
	Direct3DDemo& operator=(const Direct3DDemo& rhs) = delete;
	~Direct3DDemo();

	virtual bool Initialize()override;
	 
private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

	virtual void KeyInput(const GameTimer& gt);

	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateSkinnedCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();

	void BuildPSOs();
	void BuildFrameResources();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& ritems);
	
	array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
	vector<unique_ptr<FrameResource>> frameResources;
	FrameResource* currFrameResource = nullptr;
	int currFrameResourceIndex = 0;

	UINT cbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> rootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = nullptr;

	Scene scene;

	unordered_map<string, ComPtr<ID3DBlob>> shaders;
	unordered_map<string, ComPtr<ID3D12PipelineState>> PSOs;

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
	vector<D3D12_INPUT_ELEMENT_DESC> skinnedInputLayout;

	PassConstants mainPassCB;

	Camera* mainCamera_ = nullptr;
	bool isWireframe = false;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    try
    {
		Direct3DDemo theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

Direct3DDemo::Direct3DDemo(HINSTANCE hInstance)
: D3DApp(hInstance) 
{
}

Direct3DDemo::~Direct3DDemo()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool Direct3DDemo::Initialize()
{
	mMainWndCaption = L"d3d_Demo";
	mClientWidth = 1280;
	mClientHeight = 720;

    if(!D3DApp::Initialize())
		return false;
	
	// 초기화 명령들을 준비하기 위해 명령 목록을 재 설정한다.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	
	// 이 힙 타입에서 하나의 디스크립터가 차지하는 크기를 가져옵니다. 
	// 이 값은 하드웨어마다 다르므로 직접 쿼리해야 합니다.
	cbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	
	scene.InitScene(md3dDevice.Get(), mCommandList.Get());
	mainCamera_ = scene.GetCamera();
	mainCamera_->SetLens(0.25f * MathHelper::Pi, AspectRatio(), 0.1f, 1000.0f);

	//LoadModels();
	//LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	//BuildShapeGeometry();
	//BuildMaterials();
	//BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	// 초기화 명령을 실행한다.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// 초기화가 완료될 때까지 기다린다.
	FlushCommandQueue();

	return true;
}

void Direct3DDemo::OnResize()
{
	D3DApp::OnResize();

	// 창의 크기가 바뀌면 종횡비를 다시 갱신한다.
	// 투영 행렬을 다시 계산한다.
	if (mainCamera_)
		mainCamera_->SetLens(0.25f * MathHelper::Pi, AspectRatio(), 0.1f, 1000.0f);
}

void Direct3DDemo::Update(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();
	keyInput_.Update();
	scene.GetPlayer()->Update(dt);

	// 순환적으로 자원 프레임 배열의 다음 원소에 접근한다.
	currFrameResourceIndex = (currFrameResourceIndex + 1) % gNumFrameResources;
	currFrameResource = frameResources[currFrameResourceIndex].get();

	// GPU가 현재 프레임 자원의 명령들을 다 처리했는지 확인한다.
	// 아직 다 처리하지 않았으면 GPU가 이 울타리 지점까지의 명령들을 처리할 때까지 기다린다.
	if (currFrameResource->Fence != 0 && mFence->GetCompletedValue() < currFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(currFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateSkinnedCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}

void Direct3DDemo::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = currFrameResource->CmdListAlloc;

	// 명령 기록에 관련된 메모리의 재활용을 위해 명령할당자를 재설정한다.
	// 재설정은 GPU가 관련명령 목록을 모두 처리한 뒤 일어난다.
	ThrowIfFailed(cmdListAlloc->Reset());

	// 명령 목록을 ExcuteCommandList를 통해서 명령 대기열에 추가했다면 명령 목록을 재설정할 수 있다.
	// 명령 목록을 재설정하면 메모리가 재설정된다.
	if (isWireframe) {
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), PSOs["opaque_wireframe"].Get()));
	}
	else {
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), PSOs["opaque"].Get()));
	}

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// 후면 버퍼와 깊이 버퍼 지우기
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	mCommandList->SetGraphicsRootSignature(rootSignature.Get());
	
	ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	auto passCB = currFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(3, passCB->GetGPUVirtualAddress());

	// ---------------------------------------------------------
	
	DrawRenderItems(mCommandList.Get(), scene.GetRenderItems(RenderLayer::Opaque));

	mCommandList->SetPipelineState(PSOs["alphaTested"].Get());
	DrawRenderItems(mCommandList.Get(), scene.GetRenderItems(RenderLayer::AlphaTested));

	mCommandList->SetPipelineState(PSOs["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), scene.GetRenderItems(RenderLayer::Transparent));

	mCommandList->SetPipelineState(PSOs["skinnedOpaque"].Get());
	DrawRenderItems(mCommandList.Get(), scene.GetRenderItems(RenderLayer::Skinned));

	// ---------------------------------------------------------

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	currFrameResource->Fence = ++mCurrentFence;

	// 새 울타리 지점을 설정하는 명령을 명령 대기열에 추가한다.
	// 지금 우리는 GPU의 시간선 상에 있으므로, 
	// 새 울타리 지점은 GPU가 이 Signal() 명령까지의
	// 모든 명령을 처리하기 전까지는 설정되지 않는다.
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void Direct3DDemo::KeyInput(const GameTimer& gt)
{
	float dt = gt.DeltaTime();
	
	// Mouse
	if (keyInput_.WasMousePressed(MouseButton::LMB))
		SetCapture(mhMainWnd);
	if (keyInput_.WasMouseReleased(MouseButton::LMB))
		ReleaseCapture();

	// KeyBoard
	isWireframe = keyInput_.IsKeyDown('L');

	scene.GetPlayer()->KeyInput(keyInput_, dt);
}

void Direct3DDemo::AnimateMaterials(const GameTimer& gt) {

}

void Direct3DDemo::UpdateObjectCBs(const GameTimer& gt) {
	auto currObjectCB = currFrameResource->ObjectCB.get();
	auto& numRenderItems = scene.GetAllRenderItems();
	for (auto& e : numRenderItems) {
		// 상수들이 바뀌었을 떄에만 cbuffer 자료를 갱신한다.
		// 이러한 갱신을 프레임 자원마다 수행해야 한다.
		if (e->numFramesDirty_ > 0) {
			XMMATRIX world = XMLoadFloat4x4(&e->world_);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->texTransform_);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->objCBIndex_, objConstants);

			// 다음 프레임 자원으로 넘어간다.
			e->numFramesDirty_--;
		}
	}
}

void Direct3DDemo::UpdateSkinnedCBs(const GameTimer& gt)
{
	auto currSkinnedCB = currFrameResource->SkinnedCB.get();

	auto& skinnedModelInsts = scene.GetSkinnedModelInsts();
	for (auto& [name, skinnedModel] : skinnedModelInsts) 		{
		skinnedModel->UpdateSkinnedAnimation();
	}
	
	SkinnedConstants skinnedConstants;
	copy(skinnedModelInsts["Vanguard"]->finalTransforms_.begin(), skinnedModelInsts["Vanguard"]->finalTransforms_.end(),
		&skinnedConstants.BoneTransforms[0]);
	
	//for (int i = 0; i < 96; i++)
	//	XMStoreFloat4x4(&skinnedConstants.BoneTransforms[i], XMMatrixTranspose(XMLoadFloat4x4(&MathHelper::Identity4x4())));
	//XMStoreFloat4x4(&skinnedConstants.BoneTransforms[0], XMMatrixTranspose(XMMatrixTranslation(0.0f, 10.0f, 0.0f)));
	
	currSkinnedCB->CopyData(0, skinnedConstants);
}

void Direct3DDemo::UpdateMaterialCBs(const GameTimer& gt) {
	auto currMaterialCB = currFrameResource->MaterialCB.get();
	auto& materials = scene.GetMaterials();
	for (auto& e : materials) {
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->numFramesDirty_ > 0) {
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->matTransform_);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->diffuseAlbedo_;
			matConstants.FresnelR0 = mat->fresnelR0_;
			matConstants.Roughness = mat->roughness_;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->matCBIndex_, matConstants);

			// Next FrameResource need to be updated too.
			mat->numFramesDirty_--;
		}
	}
}

void Direct3DDemo::UpdateMainPassCB(const GameTimer& gt) {
	XMMATRIX view = scene.GetCamera()->GetView();
	XMMATRIX proj = scene.GetCamera()->GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mainPassCB.EyePosW = scene.GetCamera()->GetPosition3f();
	mainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mainPassCB.NearZ = scene.GetCamera()->GetNearZ();
	mainPassCB.FarZ = scene.GetCamera()->GetFarZ();
	mainPassCB.TotalTime = gt.TotalTime();
	mainPassCB.DeltaTime = gt.DeltaTime();
	mainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = currFrameResource->PassCB.get();
	currPassCB->CopyData(0, mainPassCB);
}

void Direct3DDemo::BuildRootSignature() {
	// Shader programs typically require resources as input (constant buffers,
	// textures, samplers).  The root signature defines the resources the shader
	// programs expect.  If we think of the shader programs as a function, and
	// the input resources as function parameters, then the root signature can be
	// thought of as defining the function signature.  

	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,  // number of descriptors
		0); // register t0

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[5];

	// Create root CBVs.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0); // register b0
	slotRootParameter[2].InitAsConstantBufferView(1); // register b1
	slotRootParameter[3].InitAsConstantBufferView(2); // register b2
	slotRootParameter[4].InitAsConstantBufferView(3); // register b3

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter, 
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr) {
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)));
}

void Direct3DDemo::BuildDescriptorHeaps() {
	// Create the SRV heap.
	auto& textures = scene.GetTextures();

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = static_cast<UINT>(textures.size());
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvDescriptorHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// Loop over all textures and create SRVs
	UINT texIndex = 0;
	for (auto& kv : textures) {
		auto& texName = kv.first;
		auto& tex = kv.second;

		auto resource = tex->Resource;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = resource->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = resource->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		md3dDevice->CreateShaderResourceView(resource.Get(), &srvDesc, hDescriptor);

		// 다음 descriptor 위치로 이동
		hDescriptor.Offset(1, cbvSrvDescriptorSize);
	}
}

void Direct3DDemo::BuildShadersAndInputLayout() {
	const D3D_SHADER_MACRO defines[] =
	{
		//"FOG", "1",
		{ NULL, NULL }
	};
	
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		//"FOG", "1",
		{ "ALPHA_TEST", "1" },
		{ NULL, NULL }
	};

	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		{ "SKINNED", "1" },
		{ NULL, NULL }
	};

	shaders["standardVS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "VS", "vs_5_1");
	shaders["skinnedVS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", skinnedDefines, "VS", "vs_5_1");
	shaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", defines, "PS", "ps_5_1");
	shaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", alphaTestDefines, "PS", "ps_5_1");

	inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	
	skinnedInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(SkinnedVertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(SkinnedVertex, Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)offsetof(SkinnedVertex, TexC), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(SkinnedVertex, BoneWeights), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, (UINT)offsetof(SkinnedVertex, BoneIndices), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void Direct3DDemo::BuildPSOs() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
	opaquePsoDesc.pRootSignature = rootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(shaders["standardVS"]->GetBufferPointer()),
		shaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(shaders["opaquePS"]->GetBufferPointer()),
		shaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&PSOs["opaque"])));


	//
	// PSO for opaque wireframe objects.
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&PSOs["opaque_wireframe"])));

	//
	// PSO for transparent objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&PSOs["transparent"])));

	//
	// PSO for alpha tested objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(shaders["alphaTestedPS"]->GetBufferPointer()),
		shaders["alphaTestedPS"]->GetBufferSize()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&PSOs["alphaTested"])));

	//
	// PSO for skinned objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedOpaquePsoDesc = opaquePsoDesc;
	skinnedOpaquePsoDesc.InputLayout = { skinnedInputLayout.data(), (UINT)skinnedInputLayout.size() };
	skinnedOpaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(shaders["skinnedVS"]->GetBufferPointer()),
		shaders["skinnedVS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedOpaquePsoDesc, IID_PPV_ARGS(&PSOs["skinnedOpaque"])));
}

void Direct3DDemo::BuildFrameResources() {
	uint32_t numRenderItems = scene.GetAllRenderItems().size();
	uint32_t numSkinnedObjects = scene.GetSkinnedModelInsts().size();
	uint32_t numMaterials = scene.GetMaterials().size();
	for (int i = 0; i < gNumFrameResources; ++i) {
		frameResources.push_back(make_unique<FrameResource>(md3dDevice.Get(),
			1, numRenderItems, numSkinnedObjects, numMaterials));
	}
}

void Direct3DDemo::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& ritems) {
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT skinnedCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
	
	auto objectCB = currFrameResource->ObjectCB->Resource();
	auto skinnedCB = currFrameResource->SkinnedCB->Resource();
	auto materialCB = currFrameResource->MaterialCB->Resource();
	
	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i) {
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->mesh_->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->mesh_->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->primitiveType_);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->material_->diffuseSrvHeapIndex_, cbvSrvDescriptorSize);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->objCBIndex_ * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = materialCB->GetGPUVirtualAddress() + ri->material_->matCBIndex_ * matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);

		if (ri->skinnedModelInst_ != nullptr) {
			D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress() + ri->skinnedCBIndex_ * skinnedCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(2, skinnedCBAddress);
		}
		else {
			cmdList->SetGraphicsRootConstantBufferView(2, 0);
		}

		cmdList->SetGraphicsRootConstantBufferView(4, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->indexCount_, 1, ri->startIndexLocation_, ri->baseVertexLocation_, 0);
	}
}

array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Direct3DDemo::GetStaticSamplers() {
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}
