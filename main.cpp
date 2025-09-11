#include "d3dApp.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"
#include "Camera.h"
#include "ModelLoader.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

struct SkinnedModelInstance
{
	SkinnedData* skinnedInfo_ = nullptr;
	vector<DirectX::XMFLOAT4X4> finalTransforms_;
	string clipName_;
	float timePos_ = 0.0f;

	// Called every frame and increments the time position, interpolates the 
	// animations for each bone based on the current animation clip, and 
	// generates the final transforms which are ultimately set to the effect
	// for processing in the vertex shader.
	void UpdateSkinnedAnimation(float dt)
	{
		timePos_ += dt;

		// Loop animation
		float animationTick = skinnedInfo_->SecondToTick(clipName_, timePos_);

		// Compute the final transforms for this time position.
		skinnedInfo_->GetFinalTransforms(clipName_, animationTick, finalTransforms_);
	}
};

// 하나의 물체를 그리는 데 필요한 매개변수들을 담는 가벼운 구조체
// 이런 구조체의 구체적인 구성은 응용 프로그램마다 다를 수 있다.
struct RenderItem {
	RenderItem() = default;

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

	Material* material_ = nullptr;
	MeshGeometry* mesh_ = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY primitiveType_ = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	BoundingBox boundBox_;

	// DrawIndexedInstanced parameters.
	uint32_t indexCount_ = 0;
	uint32_t startIndexLocation_ = 0;
	uint32_t baseVertexLocation_ = 0;

	// Only applicable to skinned render-items.
	uint32_t skinnedCBIndex_ = -1;

	// nullptr if this render-item is not animated by skinned mesh.
	SkinnedModelInstance* skinnedModelInst_ = nullptr;
};

enum class RenderLayer : uint8_t {
	Opaque = 0,
	Transparent,
	AlphaTested,
	Skinned,
	Count
};

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

	virtual void OnMouseDown(WPARAM btnState, int x, int y);
	virtual void OnMouseUp(WPARAM btnState, int x, int y);
	virtual void OnMouseMove(WPARAM btnState, int x, int y);

	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateSkinnedCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void LoadModels();
	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();

	void BuildRenderItems();
	void BuildRenderItem(uint8_t renderLayer, MeshGeometry* mesh, const Submesh& submesh, Material* material,
		const XMFLOAT4X4& worldTransform = MathHelper::Identity4x4(), const XMFLOAT4X4& texTransform = MathHelper::Identity4x4(),
		SkinnedModelInstance* skinnedModelInstance = nullptr, uint32_t skinnedCBIndex = -1);
	void BuildRenderItem(uint8_t renderLayer, MeshGeometry* mesh, const Submesh& submesh, Material* material,
		const XMMATRIX& worldTransform = XMMatrixIdentity(), const XMMATRIX& texTransform = XMMatrixIdentity(),
		SkinnedModelInstance* skinnedModelInstance = nullptr, uint32_t skinnedCBIndex = -1);

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& ritems);
	
	array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	void LoadTexture(const string& name, const wstring& fileName);
private:

	vector<unique_ptr<FrameResource>> frameResources;
	FrameResource* currFrameResource = nullptr;
	int currFrameResourceIndex = 0;

	UINT cbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> rootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = nullptr;

	unordered_map <string, pair<uint32_t, wstring>> texList;
	unordered_map<string, unique_ptr<MeshGeometry>> meshes;
	unordered_map<string, unique_ptr<Material>> materials;
	unordered_map<string, unique_ptr<Texture>> textures; 
	unordered_map<string, unique_ptr<SkinnedData>> skinnedData;
	unordered_map<string, ComPtr<ID3DBlob>> shaders;
	unordered_map<string, ComPtr<ID3D12PipelineState>> PSOs;

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
	vector<D3D12_INPUT_ELEMENT_DESC> skinnedInputLayout;

	// List of all the render items.
	vector<unique_ptr<RenderItem>> allRenderItems;

	// Render items divided by PSO.
	vector<RenderItem*> renderItemLayer[(uint8_t)RenderLayer::Count];

	PassConstants mainPassCB;

	unique_ptr<SkinnedModelInstance> skinnedModelInst;

	bool isWireframe = false;

	Camera mainCamera;

	POINT lastMousePos;
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
	mMainWndCaption = L"d3d Demo";
	mClientWidth = 1280;
	mClientHeight = 720;

    if(!D3DApp::Initialize())
		return false;
	
	// 초기화 명령들을 준비하기 위해 명령 목록을 재 설정한다.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	
	// 이 힙 타입에서 하나의 디스크립터가 차지하는 크기를 가져옵니다. 
	// 이 값은 하드웨어마다 다르므로 직접 쿼리해야 합니다.
	cbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mainCamera.SetPosition(0.0f, 2.0f, -15.0f);

	LoadModels();
	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildMaterials();
	BuildRenderItems();
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
	mainCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 0.1f, 1000.0f);
}

void Direct3DDemo::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

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
	
	DrawRenderItems(mCommandList.Get(), renderItemLayer[(uint8_t)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(PSOs["alphaTested"].Get());
	DrawRenderItems(mCommandList.Get(), renderItemLayer[(uint8_t)RenderLayer::AlphaTested]);

	mCommandList->SetPipelineState(PSOs["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), renderItemLayer[(uint8_t)RenderLayer::Transparent]);

	mCommandList->SetPipelineState(PSOs["skinnedOpaque"].Get());
	DrawRenderItems(mCommandList.Get(), renderItemLayer[(uint8_t)RenderLayer::Skinned]);

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

void Direct3DDemo::OnMouseDown(WPARAM btnState, int x, int y) {
	lastMousePos.x = x;
	lastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void Direct3DDemo::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void Direct3DDemo::OnMouseMove(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - lastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - lastMousePos.y));

		mainCamera.Pitch(dy);
		mainCamera.RotateY(dx);
	}

	lastMousePos.x = x;
	lastMousePos.y = y;
}

void Direct3DDemo::OnKeyboardInput(const GameTimer& gt) {
	const float dt = gt.DeltaTime();
	
	if (GetAsyncKeyState('1') & 0x8000)
		isWireframe = true;
	else
		isWireframe = false;

	float moveSpeed = 20.0f;
	if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
		moveSpeed *= 10.0f;

	if (GetAsyncKeyState('W') & 0x8000)
		mainCamera.Walk(moveSpeed * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mainCamera.Walk(-moveSpeed * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mainCamera.Strafe(-moveSpeed * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mainCamera.Strafe(moveSpeed * dt);

	if (GetAsyncKeyState(VK_SPACE) & 0x8000)
		mainCamera.WorldUp(moveSpeed * dt);

	if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
		mainCamera.WorldUp(-moveSpeed * dt);


	mainCamera.UpdateViewMatrix();
}

void Direct3DDemo::AnimateMaterials(const GameTimer& gt) {

}

void Direct3DDemo::UpdateObjectCBs(const GameTimer& gt) {
	auto currObjectCB = currFrameResource->ObjectCB.get();
	for (auto& e : allRenderItems) {
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

	skinnedModelInst->UpdateSkinnedAnimation(gt.DeltaTime());
	
	SkinnedConstants skinnedConstants;
	copy(skinnedModelInst->finalTransforms_.begin(), skinnedModelInst->finalTransforms_.end(),
		&skinnedConstants.BoneTransforms[0]);
	
	
	//for (int i = 0; i < 96; i++)
	//	XMStoreFloat4x4(&skinnedConstants.BoneTransforms[i], XMMatrixTranspose(XMLoadFloat4x4(&MathHelper::Identity4x4())));
	//XMStoreFloat4x4(&skinnedConstants.BoneTransforms[0], XMMatrixTranspose(XMMatrixTranslation(0.0f, 10.0f, 0.0f)));
	
	currSkinnedCB->CopyData(0, skinnedConstants);
}

void Direct3DDemo::UpdateMaterialCBs(const GameTimer& gt) {
	auto currMaterialCB = currFrameResource->MaterialCB.get();
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
	XMMATRIX view = mainCamera.GetView();
	XMMATRIX proj = mainCamera.GetProj();

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
	mainPassCB.EyePosW = mainCamera.GetPosition3f();
	mainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mainPassCB.NearZ = mainCamera.GetNearZ();
	mainPassCB.FarZ = mainCamera.GetFarZ();
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

void Direct3DDemo::LoadModels()
{
	ModelLoader modelLoader;
	modelLoader.ReadModelFile("Models/Soldier/Vanguard.fbx");
	//modelLoader.ReadAnimation("Models/Fox.glb");
	modelLoader.ReadAnimationFile("Models/Soldier/Animation/RunForward.fbx", "RunForward");

	skinnedData["skinned"] = make_unique<SkinnedData>(move(modelLoader.skinnedData_));
	skinnedModelInst = make_unique<SkinnedModelInstance>();
	skinnedModelInst.get()->skinnedInfo_ = skinnedData["skinned"].get();
	skinnedModelInst.get()->clipName_ = "RunForward0";
	skinnedModelInst.get()->finalTransforms_.resize(skinnedModelInst.get()->skinnedInfo_->BoneCount());

	const UINT vbByteSize = (UINT)modelLoader.skinnedMesh_.vertices.size() * sizeof(SkinnedVertex);
	const UINT ibByteSize = (UINT)modelLoader.skinnedMesh_.indices.size() * sizeof(uint32_t);

	auto geo = make_unique<MeshGeometry>();
	geo->Name = "skinnedGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), modelLoader.skinnedMesh_.vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), modelLoader.skinnedMesh_.indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), modelLoader.skinnedMesh_.vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), modelLoader.skinnedMesh_.indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(SkinnedVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->subMeshes_["0"] = modelLoader.subMeshes_[0];
	geo->subMeshes_["1"] = modelLoader.subMeshes_[1];

	meshes[geo->Name] = move(geo);
}

void Direct3DDemo::LoadTextures() {
	vector<pair<string, wstring>> texNames = {
		{"bricksTex",	L"Textures/d3d12/bricks.dds"},
		{"stoneTex",	L"Textures/d3d12/stone.dds"},
		{"tileTex",		L"Textures/d3d12/tile.dds"},
		{"iceTex",		L"Textures/d3d12/ice.dds"},
		{"fenceTex",	L"Textures/d3d12/WireFence.dds"},
		{"soldierTex",	L"Textures/soldier.dds"},
	};
	for (auto& [name, filepath] : texNames)
		texList.insert({ name, {texList.size(), filepath} });

	for (auto& [name, v] : texList)
		LoadTexture(name, v.second);
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
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = textures.size();
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto bricksTex = textures["bricksTex"]->Resource;
	auto stoneTex = textures["stoneTex"]->Resource;
	auto tileTex = textures["tileTex"]->Resource;
	auto iceTex = textures["iceTex"]->Resource;
	auto fenceTex = textures["fenceTex"]->Resource;
	auto soldierTex = textures["soldierTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = bricksTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = bricksTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	md3dDevice->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, cbvSrvDescriptorSize);

	srvDesc.Format = stoneTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = stoneTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(stoneTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, cbvSrvDescriptorSize);

	srvDesc.Format = tileTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = tileTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(tileTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, cbvSrvDescriptorSize);

	srvDesc.Format = iceTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = iceTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(iceTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, cbvSrvDescriptorSize);

	srvDesc.Format = fenceTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(fenceTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, cbvSrvDescriptorSize);

	srvDesc.Format = soldierTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(soldierTex.Get(), &srvDesc, hDescriptor);
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

void Direct3DDemo::BuildShapeGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	// Define the SubmeshGeometry that cover different 
	// regions of the vertex/index buffers.

	Submesh boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;
	boxSubmesh.Bounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	boxSubmesh.Bounds.Extents = XMFLOAT3(0.5f, 0.5f, 0.5f);

	geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	Submesh gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;
	gridSubmesh.Bounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	gridSubmesh.Bounds.Extents = XMFLOAT3(10.0f, 0.01f, 30.0f);

	Submesh sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;
	sphereSubmesh.Bounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	sphereSubmesh.Bounds.Extents = XMFLOAT3(0.5f, 0.5f, 0.5f);

	Submesh cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;
	cylinderSubmesh.Bounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	cylinderSubmesh.Bounds.Extents = XMFLOAT3(0.5f, 3.0f, 0.5f);
	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}

	vector<uint32_t> indices;
	indices.insert(indices.end(), begin(box.Indices32), end(box.Indices32));
	indices.insert(indices.end(), begin(grid.Indices32), end(grid.Indices32));
	indices.insert(indices.end(), begin(sphere.Indices32), end(sphere.Indices32));
	indices.insert(indices.end(), begin(cylinder.Indices32), end(cylinder.Indices32));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(uint32_t);

	auto geo = make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->subMeshes_["box"] = boxSubmesh;
	geo->subMeshes_["grid"] = gridSubmesh;
	geo->subMeshes_["sphere"] = sphereSubmesh;
	geo->subMeshes_["cylinder"] = cylinderSubmesh;

	meshes[geo->Name] = move(geo);
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
	for (int i = 0; i < gNumFrameResources; ++i) {
		frameResources.push_back(make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)allRenderItems.size(), 1, (UINT)materials.size()));
	}
}

void Direct3DDemo::BuildMaterials()
{
	materials.insert({ "bricks0",	make_unique<Material>("bricks0",	materials.size(), texList["bricksTex"].first, -1,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),	XMFLOAT3(0.02f, 0.02f, 0.02f),	0.1f) });

	materials.insert({ "stone0",	make_unique<Material>("stone0",		materials.size(), texList["stoneTex"].first, -1,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),	XMFLOAT3(0.05f, 0.05f, 0.05f),	0.3f) });

	materials.insert({ "tile0",		make_unique<Material>("tile0",		materials.size(), texList["tileTex"].first, -1,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),	XMFLOAT3(0.02f, 0.02f, 0.02f),	0.3f) });
	
	materials.insert({ "ice0",		make_unique<Material>("ice0",		materials.size(), texList["iceTex"].first, -1,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f),	XMFLOAT3(0.1f, 0.1f, 0.1f),		0.0f) });
	
	materials.insert({ "wirefence", make_unique<Material>("wirefence",	materials.size(), texList["fenceTex"].first, -1,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),	XMFLOAT3(0.1f, 0.1f, 0.1f),		0.25f) });

	materials.insert({ "soldier",	make_unique<Material>("soldier",	materials.size(), texList["soldierTex"].first, -1,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),	XMFLOAT3(0.1f, 0.1f, 0.1f),		0.25f) });
}

void Direct3DDemo::BuildRenderItems() {
	UINT objCBIndex = 0;
	
	BuildRenderItem((uint8_t)RenderLayer::AlphaTested, 
		meshes["shapeGeo"].get(), meshes["shapeGeo"].get()->subMeshes_["box"], materials["wirefence"].get(),
		XMMatrixScaling(3.0f, 3.0f, 3.0f) * XMMatrixTranslation(0.0f, 3.0f, 0.0f));

	BuildRenderItem((uint8_t)RenderLayer::Transparent,
		meshes["shapeGeo"].get(), meshes["shapeGeo"].get()->subMeshes_["box"], materials["ice0"].get(),
		XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(0.0f, 4.0f, 0.0f));

	BuildRenderItem((uint8_t)RenderLayer::Opaque,
		meshes["shapeGeo"].get(), meshes["shapeGeo"].get()->subMeshes_["grid"], materials["tile0"].get(),
		XMMatrixScaling(8.0f, 8.0f, 1.0f));

	BuildRenderItem((uint8_t)RenderLayer::Skinned,
		meshes["skinnedGeo"].get(), meshes["skinnedGeo"].get()->subMeshes_["0"], materials["soldier"].get(),
		XMMatrixScaling(0.1f, 0.1f, 0.1f), XMMatrixIdentity(),
		skinnedModelInst.get(), 0);

	BuildRenderItem((uint8_t)RenderLayer::Skinned,
		meshes["skinnedGeo"].get(), meshes["skinnedGeo"].get()->subMeshes_["1"], materials["soldier"].get(),
		XMMatrixScaling(0.1f, 0.1f, 0.1f), XMMatrixIdentity(),
		skinnedModelInst.get(), 0);
	
	//---------------------------------------
	
	for (int i = 0; i < 5; ++i) {
		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

		BuildRenderItem((uint8_t)RenderLayer::Opaque,
			meshes["shapeGeo"].get(), meshes["shapeGeo"].get()->subMeshes_["cylinder"], materials["bricks0"].get(),
			leftCylWorld);

		BuildRenderItem((uint8_t)RenderLayer::Opaque,
			meshes["shapeGeo"].get(), meshes["shapeGeo"].get()->subMeshes_["cylinder"], materials["bricks0"].get(),
			rightCylWorld);

		BuildRenderItem((uint8_t)RenderLayer::Opaque,
			meshes["shapeGeo"].get(), meshes["shapeGeo"].get()->subMeshes_["sphere"], materials["stone0"].get(),
			leftSphereWorld);

		BuildRenderItem((uint8_t)RenderLayer::Opaque,
			meshes["shapeGeo"].get(), meshes["shapeGeo"].get()->subMeshes_["sphere"], materials["stone0"].get(),
			rightSphereWorld);
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

void Direct3DDemo::LoadTexture(const string& name, const wstring& fileName)
{
	auto bricksTex = make_unique<Texture>();
	bricksTex->Name = name;
	bricksTex->Filename = fileName;
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), bricksTex->Filename.c_str(),
		bricksTex->Resource, bricksTex->UploadHeap));

	textures[bricksTex->Name] = move(bricksTex);
}

void Direct3DDemo::BuildRenderItem(uint8_t renderLayer, MeshGeometry* mesh, const Submesh& submesh, Material* material,
	const XMFLOAT4X4& worldTransform, const XMFLOAT4X4& texTransform,
	SkinnedModelInstance* skinnedModelInstance, uint32_t skinnedCBIndex)
{
	auto renderItem = make_unique<RenderItem>();
	renderItem->world_ = worldTransform;
	renderItem->texTransform_ = texTransform;
	renderItem->objCBIndex_ = allRenderItems.size();
	renderItem->material_ = material;
	renderItem->mesh_ = mesh;
	//renderItem->primitiveType_ = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	renderItem->indexCount_ = submesh.IndexCount;
	renderItem->startIndexLocation_ = submesh.StartIndexLocation;
	renderItem->baseVertexLocation_ = submesh.BaseVertexLocation;
	renderItem->skinnedModelInst_ = skinnedModelInstance;
	renderItem->skinnedCBIndex_ = skinnedCBIndex;

	renderItemLayer[renderLayer].push_back(renderItem.get());
	allRenderItems.push_back(move(renderItem));
}

void Direct3DDemo::BuildRenderItem(uint8_t renderLayer, MeshGeometry* mesh, const Submesh& submesh, Material* material,
	const XMMATRIX& worldTransform, const XMMATRIX& texTransform,
	SkinnedModelInstance* skinnedModelInstance, uint32_t skinnedCBIndex)
{
	auto renderItem = make_unique<RenderItem>();
	XMStoreFloat4x4(&renderItem->world_, worldTransform);
	XMStoreFloat4x4(&renderItem->texTransform_, texTransform);
	renderItem->objCBIndex_ = allRenderItems.size();
	renderItem->material_ = material;
	renderItem->mesh_ = mesh;
	//renderItem->primitiveType_ = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	renderItem->indexCount_ = submesh.IndexCount;
	renderItem->startIndexLocation_ = submesh.StartIndexLocation;
	renderItem->baseVertexLocation_ = submesh.BaseVertexLocation;
	renderItem->skinnedModelInst_ = skinnedModelInstance;
	renderItem->skinnedCBIndex_ = skinnedCBIndex;

	renderItemLayer[renderLayer].push_back(renderItem.get());
	allRenderItems.push_back(move(renderItem));
}