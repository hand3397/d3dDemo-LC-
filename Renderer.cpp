#include "Renderer.h"

Renderer::Renderer()
	: clientHeight_(0), clientWidth_(0), countDebugVertices_(0),
	  scissorRect_({ 0, 0, 0, 0 }), screenViewport_({ 0, 0, 0, 0, 0.0f, 1.0f })
{
}

Renderer::~Renderer()
{
	if (d3dDevice_ != nullptr)
		FlushCommandQueue();
}

ID3D12Device* Renderer::GetDevice()
{
	if (d3dDevice_)
		return d3dDevice_.Get();
	return nullptr;
}

ID3D12GraphicsCommandList* Renderer::GetCommandList()
{
	if (commandList_)
		return commandList_.Get();
	return nullptr;
}

void Renderer::CommandListReset()
{
	// 초기화 명령들을 준비하기 위해 명령 목록을 재 설정한다.
	ThrowIfFailed(commandList_->Reset(directCmdListAlloc_.Get(), nullptr));
}

void Renderer::CommandListClose()
{
	// 초기화 명령을 실행한다.
	ThrowIfFailed(commandList_->Close());
	ID3D12CommandList* cmdLists[] = { commandList_.Get() };
	commandQueue_->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// 초기화가 완료될 때까지 기다린다.
	FlushCommandQueue();
}

bool Renderer::InitDirect3D(HWND hwnd, int clientWidth, int clientHeight)
{
	clientHeight_ = clientHeight;
	clientWidth_ = clientWidth;

	if (!InitDirect3D(hwnd))
		return false;

	// 이 힙 타입에서 하나의 디스크립터가 차지하는 크기를 가져옵니다. 
	// 이 값은 하드웨어마다 다르므로 직접 쿼리해야 합니다.
	cbvSrvDescriptorSize_ = d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	OnResize(clientWidth, clientHeight);
	
	return true;
}

void Renderer::InitScene(Scene* scene)
{
	mainCamera_ = scene->GetCamera();

	BuildRootSignature();
	BuildDescriptorHeaps(scene);
	BuildShadersAndInputLayout();
	BuildFrameResources(scene);
	BuildPSOs();

	BuildDebugMesh();
}

void Renderer::OnResize(int clientWidth, int clientHeight)
{
	clientWidth_ = clientWidth;
	clientHeight_ = clientHeight;

	screenViewport_.TopLeftX = 0;
	screenViewport_.TopLeftY = 0;
	screenViewport_.Width = static_cast<float>(clientWidth);
	screenViewport_.Height = static_cast<float>(clientHeight);
	screenViewport_.MinDepth = 0.0f;
	screenViewport_.MaxDepth = 1.0f;

	scissorRect_ = { 0, 0, clientWidth, clientHeight };

	assert(d3dDevice_);
	assert(swapChain_);
	assert(directCmdListAlloc_);

	// Flush before changing any resources.
	FlushCommandQueue();

	ThrowIfFailed(commandList_->Reset(directCmdListAlloc_.Get(), nullptr));

	// Release the previous resources we will be recreating.
	for (int i = 0; i < numSwapChainBuffers_; ++i)
		swapChainBuffer_[i].Reset();
	depthStencilBuffer_.Reset();

	// Resize the swap chain.
	ThrowIfFailed(swapChain_->ResizeBuffers(
		numSwapChainBuffers_,
		clientWidth_, clientHeight_,
		backBufferFormat_,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	currBackBuffer_ = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(rtvHeap_->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < numSwapChainBuffers_; i++) {
		ThrowIfFailed(swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainBuffer_[i])));
		d3dDevice_->CreateRenderTargetView(swapChainBuffer_[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, rtvDescriptorSize_);
	}

	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc = {};
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = clientWidth_;
	depthStencilDesc.Height = clientHeight_;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;

	// SSAO 챕터에서는 깊이 버퍼를 읽기 위해 SRV가 필요합니다.
	// 따라서 같은 리소스에 두 개의 뷰를 생성해야 합니다:
	//   1. SRV 형식: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
	//   2. DSV 형식: DXGI_FORMAT_D24_UNORM_S8_UINT
	// 따라서 깊이 버퍼 리소스는 typeless 형식으로 생성합니다.
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

	depthStencilDesc.SampleDesc.Count = msaaState_ ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = msaaState_ ? (msaaQuality_ - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear = {};
	optClear.Format = depthStencilFormat_;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	ThrowIfFailed(d3dDevice_->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(depthStencilBuffer_.GetAddressOf())));

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = depthStencilFormat_;
	dsvDesc.Texture2D.MipSlice = 0;
	d3dDevice_->CreateDepthStencilView(depthStencilBuffer_.Get(), &dsvDesc, DepthStencilView());

	// Transition the resource from its initial state to be used as a depth buffer.
	commandList_->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(depthStencilBuffer_.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Execute the resize commands.
	ThrowIfFailed(commandList_->Close());
	ID3D12CommandList* cmdsLists[] = { commandList_.Get() };
	commandQueue_->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();
}

void Renderer::Update(const GameTimer& gt, Scene* scene)
{
	const float dt = gt.DeltaTime();

	// 순환적으로 자원 프레임 배열의 다음 원소에 접근한다.
	currFrameResourceIndex_ = (currFrameResourceIndex_ + 1) % NUM_FRAME_RESOURCES;
	currFrameResource_ = frameResources_[currFrameResourceIndex_].get();

	// GPU가 현재 프레임 자원의 명령들을 다 처리했는지 확인한다.
	// 아직 다 처리하지 않았으면 GPU가 이 울타리 지점까지의 명령들을 처리할 때까지 기다린다.
	if (currFrameResource_ ->fence_ != 0
		&& fence_->GetCompletedValue() < currFrameResource_->fence_) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(fence_->SetEventOnCompletion(currFrameResource_->fence_, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs(gt, scene);
	UpdateInstanceData(gt, scene);
	UpdateSkinnedCBs(gt, scene);
	UpdateMaterialBuffer(gt, scene);
	UpdateMainPassCB(gt, scene);
	
	UpdateDebugMesh(scene);
}

void Renderer::Draw(const Scene* scene)
{
	auto& cmdListAlloc = currFrameResource_->cmdListAlloc_;

	// 명령 기록에 관련된 메모리의 재활용을 위해 명령할당자를 재설정한다.
	// 재설정은 GPU가 관련명령 목록을 모두 처리한 뒤 일어난다.
	ThrowIfFailed(cmdListAlloc->Reset());

	// 명령 목록을 ExcuteCommandList를 통해서 명령 대기열에 추가했다면 명령 목록을 재설정할 수 있다.
	// 명령 목록을 재설정하면 메모리가 재설정된다.
	ThrowIfFailed(commandList_->Reset(cmdListAlloc.Get(), PSOs_["opaque"].Get()));

	commandList_->RSSetViewports(1, &screenViewport_);
	commandList_->RSSetScissorRects(1, &scissorRect_);

	// Indicate a state transition on the resource usage.
	commandList_->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// 후면 버퍼와 깊이 버퍼 지우기
	commandList_->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	commandList_->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	commandList_->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	commandList_->SetGraphicsRootSignature(rootSignature_.Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_.Get() };
	commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	auto passCB = currFrameResource_->passCB_->Resource();
	commandList_->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());
	
	auto instanceBuffer = currFrameResource_->instanceBuffer_->Resource();
	commandList_->SetGraphicsRootShaderResourceView(3, instanceBuffer->GetGPUVirtualAddress());

	// Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
	// set as a root descriptor.
	auto matBuffer = currFrameResource_->materialBuffer_->Resource();
	commandList_->SetGraphicsRootShaderResourceView(4, matBuffer->GetGPUVirtualAddress());

	// Slot 5: 모든 일반 텍스처 (space0)
	commandList_->SetGraphicsRootDescriptorTable(5, srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart());

	// Slot 6: 지형 전용 Texture2DArray (space1)
	// 힙의 가장 마지막 칸 주소를 계산해서 바인딩합니다.
	auto hDescriptorStart = srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart();
	int terrainIdx = static_cast<int>(scene->GetTextures().size()) - 1;

	CD3DX12_GPU_DESCRIPTOR_HANDLE hTerrain(hDescriptorStart);
	hTerrain.Offset(terrainIdx, cbvSrvDescriptorSize_);

	commandList_->SetGraphicsRootDescriptorTable(6, hTerrain);

	// ---------------------------------------------------------
    // Render the scene objects.
	// ---------------------------------------------------------

	DrawRenderItems(scene->GetRenderItems(RenderLayer::RENDER_OPAQUE));
	
	commandList_->SetPipelineState(PSOs_["texArrayOpaque"].Get());
	DrawRenderItems(scene->GetRenderItems(RenderLayer::RENDER_TEX_ARRAY_OPAQUE));
	
	//commandList_->SetPipelineState(PSOs_["skinnedOpaque"].Get());
	//DrawRenderItems(scene->GetRenderItems(RenderLayer::RENDER_SKINNED));

	commandList_->SetPipelineState(PSOs_["instance"].Get());
	DrawRenderItems(scene->GetRenderItems(RenderLayer::RENDER_INSTANCE));
	
	commandList_->SetPipelineState(PSOs_["alphaTested"].Get());
	DrawRenderItems(scene->GetRenderItems(RenderLayer::RENDER_ALPHATESTED));
	
	commandList_->SetPipelineState(PSOs_["billboard"].Get());
	DrawRenderItems(scene->GetRenderItems(RenderLayer::RENDER_ALPHATESTED_BILLBOARD));
	
	commandList_->SetPipelineState(PSOs_["transparent"].Get());
	DrawRenderItems(scene->GetRenderItems(RenderLayer::RENDER_TRANSPARENT));

	commandList_->SetPipelineState(PSOs_["color"].Get());
	DrawDebugBox();
	// ---------------------------------------------------------

	// Indicate a state transition on the resource usage.
	commandList_->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(commandList_->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { commandList_.Get() };
	commandQueue_->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the back and front buffers
	ThrowIfFailed(swapChain_->Present(0, 0));
	currBackBuffer_ = (currBackBuffer_ + 1) % numSwapChainBuffers_;

	currFrameResource_->fence_ = ++currentFence_;

	// 새 울타리 지점을 설정하는 명령을 명령 대기열에 추가한다.
	// 지금 우리는 GPU의 시간선 상에 있으므로, 
	// 새 울타리 지점은 GPU가 이 Signal() 명령까지의
	// 모든 명령을 처리하기 전까지는 설정되지 않는다.
	commandQueue_->Signal(fence_.Get(), currentFence_);
}

void Renderer::BuildRootSignature()
{
	// Shader programs typically require resources as input (constant buffers,
	// textures, samplers).  The root signature defines the resources the shader
	// programs expect.  If we think of the shader programs as a function, and
	// the input resources as function parameters, then the root signature can be
	// thought of as defining the function signature.  

	// 1. 일반 텍스처 (t0, space0) -> HLSL의 gDiffuseMap[16]
	CD3DX12_DESCRIPTOR_RANGE texTableObj[1] = {};
	texTableObj[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 0, 0);

	// 2. 지형 Array (t0, space1) -> HLSL의 gDiffuseMapArray
	CD3DX12_DESCRIPTOR_RANGE texTableTerrain[1] = {};
	texTableTerrain[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1);

	CD3DX12_ROOT_PARAMETER slotRootParameter[7] = {};

	slotRootParameter[0].InitAsConstantBufferView(0);    // b0, space0
	slotRootParameter[1].InitAsConstantBufferView(1);    // b1, space0
	slotRootParameter[2].InitAsConstantBufferView(2);    // b2, space0

	// 3. instanceData (t16, space0) -> HLSL의 StructuredBuffer 대응
	slotRootParameter[3].InitAsShaderResourceView(16, 0);
	// 4. matData (t16, space1) -> HLSL의 gMaterialData : register(t16, space1)
	// **중요**: 여기가 t16, space1이어야 셰이더의 t16, space1과 매칭됩니다.
	slotRootParameter[4].InitAsShaderResourceView(16, 1);

	// 5. Descriptor Table (space0) -> 일반 텍스처용
	slotRootParameter[5].InitAsDescriptorTable(1, texTableObj, D3D12_SHADER_VISIBILITY_PIXEL);
	// 6. Descriptor Table (space1) -> 지형 Array용
	slotRootParameter[6].InitAsDescriptorTable(1, texTableTerrain, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(7, slotRootParameter,
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

	ThrowIfFailed(d3dDevice_->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_)));
}

void Renderer::BuildDescriptorHeaps(Scene* scene)
{
	const auto& textures = scene->GetTextures();
	
	if (textures.empty()) 
		return;

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = static_cast<UINT>(textures.size());
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(d3dDevice_->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvDescriptorHeap_)));

	const D3D12_CPU_DESCRIPTOR_HANDLE hStart = srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

	for (auto& [name, tex] : textures) {
		const auto& resource = tex->resource_;
		const D3D12_RESOURCE_DESC resDesc = resource->GetDesc();

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = resDesc.Format;

		if (resDesc.DepthOrArraySize > 1) {
			// Texture 2D Array
            // 마지막 texture는 Texture2DArray이므로 SRV를 생성할 때 뷰 차원을 D3D12_SRV_DIMENSION_TEXTURE2DARRAY로 설정한다.
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MostDetailedMip = 0;
			srvDesc.Texture2DArray.MipLevels = resDesc.MipLevels;
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.ArraySize = resDesc.DepthOrArraySize;
		}
		else {
			// Texture 2D
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = resDesc.MipLevels;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(hStart, tex->srvHeapIndex_, cbvSrvDescriptorSize_);
		d3dDevice_->CreateShaderResourceView(resource.Get(), &srvDesc, hDescriptor);
	}
}

void Renderer::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO defines[] =
	{
		//"FOG", "1",
		{ NULL, NULL }
	};

	const D3D_SHADER_MACRO texArrayDefines[] =
	{
		{ "TEX_ARRAY", "1" },
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

	shaders_["colorVS"]			= d3dUtil::CompileShader(L"Shaders/color.hlsl",		nullptr,			"VS", "vs_5_1");
	shaders_["standardVS"]		= d3dUtil::CompileShader(L"Shaders/Default.hlsl",	nullptr,			"VS", "vs_5_1");
	shaders_["texArrayVS"]		= d3dUtil::CompileShader(L"Shaders/Default.hlsl",	texArrayDefines,	"VS", "vs_5_1");
	shaders_["skinnedVS"]		= d3dUtil::CompileShader(L"Shaders/Default.hlsl",	skinnedDefines,		"VS", "vs_5_1");
	shaders_["instanceVS"]		= d3dUtil::CompileShader(L"Shaders/Instance.hlsl",	defines,			"VS", "vs_5_1");
	shaders_["billboardVS"]		= d3dUtil::CompileShader(L"Shaders/Billboard.hlsl", defines,			"VS", "vs_5_1");

	shaders_["billboardGS"]		= d3dUtil::CompileShader(L"Shaders/Billboard.hlsl", nullptr,			"GS", "gs_5_1");

	shaders_["colorPS"]			= d3dUtil::CompileShader(L"Shaders/color.hlsl",		nullptr,			"PS", "ps_5_1");
	shaders_["opaquePS"]		= d3dUtil::CompileShader(L"Shaders/Default.hlsl",	defines,			"PS", "ps_5_1");
	shaders_["texArrayOpaquePS"]= d3dUtil::CompileShader(L"Shaders/Default.hlsl",	texArrayDefines,	"PS", "ps_5_1");
	shaders_["instancePS"]		= d3dUtil::CompileShader(L"Shaders/Instance.hlsl",	defines,			"PS", "ps_5_1");
	shaders_["alphaTestedPS"]	= d3dUtil::CompileShader(L"Shaders/Default.hlsl",	alphaTestDefines,	"PS", "ps_5_1");

	inputLayouts_["standard"] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)offsetof(Vertex, TexC), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	
	inputLayouts_["texArray"] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(VertexTexArray, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(VertexTexArray, Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)offsetof(VertexTexArray, TexC), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXINDEX", 0, DXGI_FORMAT_R32_UINT, 0, (UINT)offsetof(VertexTexArray, TexArrayIndex), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	inputLayouts_["color"] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(ColorVertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, (UINT)offsetof(ColorVertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	inputLayouts_["skinned"] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(SkinnedVertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(SkinnedVertex, Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)offsetof(SkinnedVertex, TexC), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(SkinnedVertex, BoneWeights), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, (UINT)offsetof(SkinnedVertex, BoneIndices), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	inputLayouts_["billboard"] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(BillboardVertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)offsetof(BillboardVertex, Size), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void Renderer::BuildFrameResources(Scene* scene)
{
	uint32_t numRenderItems = scene->MaxNumGameObjects();
	uint32_t numInstances = scene->GetNumInstances();
	numInstances = (numInstances < 1 ? 1 : numInstances);
	uint32_t numSkinnedObjects = scene->GetSkinnedModelInsts().size();
	uint32_t numMaterials = scene->GetMaterials().size();
	for (int i = 0; i < NUM_FRAME_RESOURCES; ++i) {
		frameResources_.push_back(make_unique<FrameResource>(d3dDevice_.Get(),
			1, numRenderItems, numSkinnedObjects, numInstances, numMaterials));
	}
}

void Renderer::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc = {};

	//
	// PSO for opaque objects.
	//

	opaquePsoDesc.InputLayout = { inputLayouts_["standard"].data(), (UINT)inputLayouts_["standard"].size() };
	opaquePsoDesc.pRootSignature = rootSignature_.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(shaders_["standardVS"]->GetBufferPointer()),
		shaders_["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(shaders_["opaquePS"]->GetBufferPointer()),
		shaders_["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = backBufferFormat_;
	opaquePsoDesc.SampleDesc.Count = msaaState_ ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = msaaState_ ? (msaaQuality_ - 1) : 0;
	opaquePsoDesc.DSVFormat = depthStencilFormat_;
	ThrowIfFailed(d3dDevice_->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&PSOs_["opaque"])));

	//
	// PSO for opaque wireframe objects.
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC colorWireFramePsoDesc = opaquePsoDesc;
	colorWireFramePsoDesc.InputLayout = { inputLayouts_["color"].data(), (UINT)inputLayouts_["color"].size() };
	colorWireFramePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	colorWireFramePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	colorWireFramePsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	colorWireFramePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(shaders_["colorVS"]->GetBufferPointer()),
		shaders_["colorVS"]->GetBufferSize()
	};
	colorWireFramePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(shaders_["colorPS"]->GetBufferPointer()),
		shaders_["colorPS"]->GetBufferSize()
	};
	ThrowIfFailed(d3dDevice_->CreateGraphicsPipelineState(&colorWireFramePsoDesc, IID_PPV_ARGS(&PSOs_["color"])));

	//
	// PSO for texArraay & opaque wireframe objects.
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC texArrayOpaquePsoDesc = opaquePsoDesc;
	texArrayOpaquePsoDesc.InputLayout = { inputLayouts_["texArray"].data(), (UINT)inputLayouts_["texArray"].size() };
	texArrayOpaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(shaders_["texArrayVS"]->GetBufferPointer()),
		shaders_["texArrayVS"]->GetBufferSize()
	};
	texArrayOpaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(shaders_["texArrayOpaquePS"]->GetBufferPointer()),
		shaders_["texArrayOpaquePS"]->GetBufferSize()
	};
	ThrowIfFailed(d3dDevice_->CreateGraphicsPipelineState(&texArrayOpaquePsoDesc, IID_PPV_ARGS(&PSOs_["texArrayOpaque"])));

	//
	// PSO for opaque instance objects.
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueInstancePsoDesc = opaquePsoDesc;
	opaqueInstancePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(shaders_["instanceVS"]->GetBufferPointer()),
		shaders_["instanceVS"]->GetBufferSize()
	};
	opaqueInstancePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(shaders_["instancePS"]->GetBufferPointer()),
		shaders_["instancePS"]->GetBufferSize()
	};
	ThrowIfFailed(d3dDevice_->CreateGraphicsPipelineState(&opaqueInstancePsoDesc, IID_PPV_ARGS(&PSOs_["instance"])));

	//
	// PSO for transparent objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc = {};
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
	ThrowIfFailed(d3dDevice_->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&PSOs_["transparent"])));

	//
	// PSO for alpha tested objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(shaders_["alphaTestedPS"]->GetBufferPointer()),
		shaders_["alphaTestedPS"]->GetBufferSize()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(d3dDevice_->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&PSOs_["alphaTested"])));

	//
	// PSO for skinned objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedOpaquePsoDesc = opaquePsoDesc;
	skinnedOpaquePsoDesc.InputLayout = { inputLayouts_["skinned"].data(), (UINT)inputLayouts_["skinned"].size() };
	skinnedOpaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(shaders_["skinnedVS"]->GetBufferPointer()),
		shaders_["skinnedVS"]->GetBufferSize()
	};
	ThrowIfFailed(d3dDevice_->CreateGraphicsPipelineState(&skinnedOpaquePsoDesc, IID_PPV_ARGS(&PSOs_["skinnedOpaque"])));

	//
    // PSO for billboard objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC billboardPsoDesc = opaquePsoDesc;
	billboardPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(shaders_["billboardVS"]->GetBufferPointer()),
		shaders_["billboardVS"]->GetBufferSize()
	};
	billboardPsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(shaders_["billboardGS"]->GetBufferPointer()),
		shaders_["billboardGS"]->GetBufferSize()
	};
	billboardPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(shaders_["alphaTestedPS"]->GetBufferPointer()),
		shaders_["alphaTestedPS"]->GetBufferSize()
	};
	billboardPsoDesc.DepthStencilState.DepthEnable = false;
    billboardPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; // 빌보드의 입력은 점이므로, 점으로 설정한다.(기하셰이더로 빌보드 사각형 생성)
	billboardPsoDesc.InputLayout = { inputLayouts_["billboard"].data(), (UINT)inputLayouts_["billboard"].size() };
	billboardPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	ThrowIfFailed(d3dDevice_->CreateGraphicsPipelineState(&billboardPsoDesc, IID_PPV_ARGS(&PSOs_["billboard"])));
}

void Renderer::BuildDebugMesh()
{
	debugMesh_.vertexBufferUploader_;
	
	ThrowIfFailed(d3dDevice_->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(maxDebugVertices_ * sizeof(ColorVertex)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(debugMesh_.vertexBufferUploader_.GetAddressOf())));

	// Persistent map
	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(debugMesh_.vertexBufferUploader_->Map(
		0, &readRange, reinterpret_cast<void**>(&mappedData_)));
}

// 디버그용 메시나 선 그리기
void Renderer::UpdateDebugMesh(Scene* scene)
{
	vector<ColorVertex> vertices;

	auto pushEdge = [&](const XMFLOAT3 corner[8], int a, int b, const XMVECTOR& color) {
		vertices.emplace_back(corner[a], color);
		vertices.emplace_back(corner[b], color); };
	auto pushLine = [&](const XMFLOAT3& a, const XMFLOAT3& b, const XMVECTOR& color) {
		vertices.emplace_back(a, color);
		vertices.emplace_back(b, color); };
	auto CreateBox = [&](const XMFLOAT3 corner[8], const XMVECTOR& color) {
		pushEdge(corner, 0, 1, color); pushEdge(corner, 1, 2, color); pushEdge(corner, 2, 3, color); pushEdge(corner, 3, 0, color); // bottom
		pushEdge(corner, 4, 5, color); pushEdge(corner, 5, 6, color); pushEdge(corner, 6, 7, color); pushEdge(corner, 7, 4, color); // top
		pushEdge(corner, 0, 4, color); pushEdge(corner, 1, 5, color); pushEdge(corner, 2, 6, color); pushEdge(corner, 3, 7, color); // sides 
		};
	auto CreatePoint = [&](const XMFLOAT3& pos, const XMVECTOR& color) {
		BoundingBox bbb;
		XMFLOAT3 cornerEx[8];
		bbb.Center = pos;
		bbb.Extents = XMFLOAT3(0.01f, 0.01f, 0.01f);
		bbb.GetCorners(cornerEx);
		CreateBox(cornerEx, color);
		};
	auto CreateMesh = [&](const vector<XMVECTOR>& pos, const vector<size_t>&faces, const XMVECTOR& color) {
		int numFaces = faces.size();
		for (int i = 0; i < numFaces; i+=3) {
			XMFLOAT3 a, b, c;
			XMStoreFloat3(&a, pos[faces[i]]);
			XMStoreFloat3(&b, pos[faces[i + 1]]);
			XMStoreFloat3(&c, pos[faces[i + 2]]);
			pushLine(a, b, color);
			pushLine(b, c, color);
			pushLine(a, c, color);
		}
		};

	auto pushAABB = [&](const XMFLOAT3& lower, const XMFLOAT3& upper, const XMVECTOR& color) {
		XMFLOAT3 corner[8];

		// 8개의 꼭짓점 만들기
		corner[0] = { lower.x, lower.y, lower.z };
		corner[1] = { upper.x, lower.y, lower.z };
		corner[2] = { upper.x, upper.y, lower.z };
		corner[3] = { lower.x, upper.y, lower.z };

		corner[4] = { lower.x, lower.y, upper.z };
		corner[5] = { upper.x, lower.y, upper.z };
		corner[6] = { upper.x, upper.y, upper.z };
		corner[7] = { lower.x, upper.y, upper.z };

		// 아래 사각형
		pushEdge(corner, 0, 1, color);
		pushEdge(corner, 1, 2, color);
		pushEdge(corner, 2, 3, color);
		pushEdge(corner, 3, 0, color);

		// 위 사각형
		pushEdge(corner, 4, 5, color);
		pushEdge(corner, 5, 6, color);
		pushEdge(corner, 6, 7, color);
		pushEdge(corner, 7, 4, color);

		// 세로 엣지
		pushEdge(corner, 0, 4, color);
		pushEdge(corner, 1, 5, color);
		pushEdge(corner, 2, 6, color);
		pushEdge(corner, 3, 7, color);
		};

	// staticObjects가 실제로 존재해도 나오지 않는 문제가 있음
	const auto& staticObjects = scene->GetGameObjects(spe::RigidbodyType::STATIC);
	const auto& dynamicObjects = scene->GetGameObjects(spe::RigidbodyType::DYNAMIC);
    const auto& allRenderItems = scene->GetAllRenderItems();

	for (const auto& ri : allRenderItems) {
		BoundingBox worldBox;
		ri->boundingBox_.Transform(worldBox, XMLoadFloat4x4(&ri->world_));
		XMFLOAT3 worldCorners[8];
		worldBox.GetCorners(worldCorners);
		CreateBox(worldCorners, DirectX::Colors::DarkBlue);
	}

	auto player = scene->GetPlayer();
	/*
	if (player && (vertices.size() + 24 <= maxDebugVertices_)) {
		bool isCollide = false;

		for (auto& go : staticObjects) {
			if (player->GetRigidbody().boundingSphere_.Intersects(go->GetRigidbody().boundingSphere_))
				if (player->GetRigidbody().boundingBox_.Intersects(go->GetRigidbody().boundingBox_)) {
					isCollide = true;
					break;
				}
		}

		player->GetRigidbody().boundingBox_.GetCorners(corner);
		if (isCollide)
			CreateBox(corner, red);
		else
			CreateBox(corner, orange);
	}
	*/

	for (auto& go : staticObjects) {
		spe::AABB aabb = go->GetAABB();
		pushAABB(aabb.lowerBound, aabb.upperBound, DirectX::Colors::DarkRed);
	}

	// Contact DynamicToStatic
	for (GameObject* go : dynamicObjects) {
		spe::AABB aabb = go->GetAABB();
		if (go->GetRigidbody()->HasFlag(spe::RigidbodyFlag::AWAKE))
			pushAABB(aabb.lowerBound, aabb.upperBound, DirectX::Colors::DarkRed);
		else
			pushAABB(aabb.lowerBound, aabb.upperBound, DirectX::Colors::DarkBlue);
	}

	//---------------------------
	CreatePoint(XMFLOAT3(0.f, 0.f, 0.f), DirectX::Colors::Black);
	pushLine(XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(5.f, 0.f, 0.f), DirectX::Colors::Red);
	pushLine(XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.f, 5.f, 0.f), DirectX::Colors::Green);
	pushLine(XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.f, 0.f, 5.f), DirectX::Colors::Blue);

	// pickingRay
	/*
	XMFLOAT3 p1, p2;
	XMStoreFloat3(&p1, XMLoadFloat3(&scene->ray.vertex));
	XMStoreFloat3(&p2, XMLoadFloat3(&scene->ray.vertex) + XMLoadFloat3(&scene->ray.dir) * 1000);
	pushLine(p1, p2, DirectX::Colors::Orange);
	*/
	//---------------------------

	/*
	for (auto& go : staticObjects)
	{
		if (vertices.size() + 24 > maxDebugVertices_)
			break;
		go->GetRigidbody().boundingBox_.GetCorners(corner);
		CreateBox(corner, blue);
	}
	*/
	memcpy(mappedData_, vertices.data(), sizeof(ColorVertex) * vertices.size());

	debugMesh_.vertexByteStride_ = sizeof(ColorVertex);
	debugMesh_.vertexBufferByteSize_ = sizeof(ColorVertex) * vertices.size();
	countDebugVertices_ = vertices.size();
}

array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Renderer::GetStaticSamplers()
{
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

bool Renderer::InitDirect3D(HWND hwnd)
{
#if defined(DEBUG) || defined(_DEBUG) 
	// D3D12 디버그 레이어를 활성화합니다.
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory_)));

	// 하드웨어 디바이스를 생성 시도합니다.
	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,             // 기본 어댑터
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&d3dDevice_));

// 실패하면 WARP 장치로 폴백합니다.
	if (FAILED(hardwareResult)) {
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(dxgiFactory_->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&d3dDevice_)));
	}

	// GPU 명령 동기화를 위한 펜스를 생성합니다.
	ThrowIfFailed(d3dDevice_->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&fence_)));

	// 각 디스크립터 크기를 가져옵니다.
	rtvDescriptorSize_ = d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	dsvDescriptorSize_ = d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	cbvSrvUavDescriptorSize_ = d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 백버퍼 포맷에 대해 4X MSAA 품질 지원 여부 확인
	// Direct3D 11 지원 장치에서는 모든 렌더 타겟 포맷에 대해 4X MSAA 지원
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels = {};
	msQualityLevels.Format = backBufferFormat_;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(d3dDevice_->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

	msaaQuality_ = msQualityLevels.NumQualityLevels;
	assert(msaaQuality_ > 0 && "예상치 못한 MSAA 품질 수준입니다.");

#ifdef _DEBUG
// 어댑터 정보 로그 출력
LogAdapters();
#endif

	// 명령 큐, 명령 리스트 등 생성
	CreateCommandObjects();
	// 스왑체인 생성
	CreateSwapChain(hwnd);
	// RTV/DSV 디스크립터 힙 생성
	CreateRtvAndDsvDescriptorHeaps();

return true;
}

void Renderer::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(d3dDevice_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_)));

	ThrowIfFailed(d3dDevice_->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(directCmdListAlloc_.GetAddressOf())));

	ThrowIfFailed(d3dDevice_->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		directCmdListAlloc_.Get(), // Associated command allocator
		nullptr,                   // Initial PipelineStateObject
		IID_PPV_ARGS(commandList_.GetAddressOf())));

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	commandList_->Close();
}

void Renderer::CreateSwapChain(HWND hwnd)
{
	// Release the previous swapchain
	swapChain_.Reset();

	// 1. D3D12용 SwapChainDesc
	DXGI_SWAP_CHAIN_DESC1 sd = {};
	sd.Width = clientWidth_;
	sd.Height = clientHeight_;
	sd.Format = backBufferFormat_;
	sd.Stereo = FALSE;
	sd.SampleDesc.Count = msaaState_ ? 4 : 1;
	sd.SampleDesc.Quality = msaaState_ ? (msaaQuality_ - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = numSwapChainBuffers_;
	sd.Scaling = DXGI_SCALING_STRETCH;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// 2. SwapChain 생성
	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(dxgiFactory_->CreateSwapChainForHwnd(
		commandQueue_.Get(), // D3D12 커맨드 큐
		hwnd,
		&sd,
		nullptr,  // fullscreen desc
		nullptr,  // restrict output
		&swapChain1
	));

	// 3. IDXGISwapChain3로 변환
	ThrowIfFailed(swapChain1.As(&swapChain_));

	// 4. ALT+ENTER fullscreen toggle 방지
	dxgiFactory_->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
}

void Renderer::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = numSwapChainBuffers_;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice_->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(rtvHeap_.GetAddressOf())));


	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice_->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(dsvHeap_.GetAddressOf())));
}

void Renderer::FlushCommandQueue()
{
	// 펜스(fence) 값을 증가시켜 현재 펜스 지점까지의 명령들을 표시합니다.
	currentFence_++;

	// 명령 큐에 새로운 펜스 지점을 설정하는 명령을 추가합니다.
	// GPU 타임라인 상에서, 새로운 펜스 지점은 이전의 모든 명령이
	// 처리될 때까지 설정되지 않습니다.
	ThrowIfFailed(commandQueue_->Signal(fence_.Get(), currentFence_));

	// GPU가 현재 펜스 지점까지의 명령을 완료할 때까지 대기합니다.
	if (fence_->GetCompletedValue() < currentFence_) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		// GPU가 현재 펜스에 도달하면 이벤트를 발생시킵니다.
		ThrowIfFailed(fence_->SetEventOnCompletion(currentFence_, eventHandle));

		// GPU가 현재 펜스에 도달할 때까지 대기합니다.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

ID3D12Resource* Renderer::CurrentBackBuffer()const
{
	return swapChainBuffer_[currBackBuffer_].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::CurrentBackBufferView()const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		rtvHeap_->GetCPUDescriptorHandleForHeapStart(),
		currBackBuffer_,
		rtvDescriptorSize_);
}

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::DepthStencilView()const
{
	return dsvHeap_->GetCPUDescriptorHandleForHeapStart();
}

void Renderer::UpdateObjectCBs(const GameTimer& gt, Scene* scene)
{
	auto currObjectCB = currFrameResource_->objectCB_.get();
	const auto& renderItems = scene->GetAllRenderItems();
	for (const auto& e : renderItems) {
		// 상수들이 바뀌었을 떄에만 cbuffer 자료를 갱신한다.
		// 이러한 갱신을 프레임 자원마다 수행해야 한다.
		if (e->numFramesDirty_ > 0) {
			XMMATRIX world = XMLoadFloat4x4(&e->world_);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->texTransform_);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.world_, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.texTransform_, XMMatrixTranspose(texTransform));
			objConstants.materialIndex_ = e->material_->matCBIndex_;
			
            objConstants.atlasIndex_ = e->atlasIndex_;
			// billboard Data
            objConstants.isBillboardYAxisFixed_ = e->isBillboardYAxisFixed_ ? 1 : 0;

			if (e->isBillboardYAxisFixed_) {
				objConstants.atlasIndex_ = gt.TotalTime() * 10.0f;
				e->numFramesDirty_ = NUM_FRAME_RESOURCES + 1;
            }

			currObjectCB->CopyData(e->objCBIndex_, objConstants);

			// 다음 프레임 자원으로 넘어간다.
			e->numFramesDirty_--;
		}
	}
}

void Renderer::UpdateInstanceData(const GameTimer& gt, Scene* scene)
{
	BoundingFrustum worldFrustum = mainCamera_->GetBoundingFrustum();
	XMMATRIX invView = XMMatrixInverse(nullptr, mainCamera_->GetView());
	worldFrustum.Transform(worldFrustum, invView);

	auto currInstanceBuffer = currFrameResource_->instanceBuffer_.get();
	auto& renderItems = scene->GetRenderItems(RenderLayer::RENDER_INSTANCE);
	for (auto& e : renderItems) {
		const auto& instanceData = e->instances_;

		if (e->instances_.empty())
			continue;

		BoundingBox boundingBox = e->boundingBox_;
		BoundingSphere boundingSphere = e->boundingSphere_;

		int visibleInstanceCount = 0;
		for (UINT i = 0; i < (UINT)instanceData.size(); ++i) {
			XMMATRIX world = XMLoadFloat4x4(&instanceData[i].world_);

			BoundingSphere bs;
			boundingSphere.Transform(bs, world);
			if (worldFrustum.Contains(bs) == DISJOINT)
				continue;

			BoundingBox bb;
			boundingBox.Transform(bb, world);
			if (worldFrustum.Contains(bb) == DISJOINT)
				continue;

			XMMATRIX texTransform = XMLoadFloat4x4(&instanceData[i].texTransform_);
			InstanceData data;

			XMStoreFloat4x4(&data.world_, XMMatrixTranspose(world));
			XMStoreFloat4x4(&data.texTransform_, XMMatrixTranspose(texTransform));
			data.materialIndex_ = instanceData[i].materialIndex_;

			// Write the instance data to structured buffer for the visible objects.
			currInstanceBuffer->CopyData(visibleInstanceCount++, data);
		}
		e->instanceCount_ = visibleInstanceCount;
		
		/*
		std::wostringstream outs;
		outs.precision(6);
		outs << L"Instancing and Culling Demo" <<
			L"    " << e->instanceCount_ <<
			L" objects visible out of " << e->instances_.size() << '\n';
		OutputDebugStringW(outs.str().c_str());
		*/
	}
}

void Renderer::UpdateSkinnedCBs(const GameTimer& gt, Scene* scene)
{
	auto currSkinnedCB = currFrameResource_->skinnedCB_.get();

	auto& skinnedModelInsts = scene->GetSkinnedModelInsts();
	for (auto& [name, skinnedModel] : skinnedModelInsts) {
		skinnedModel->UpdateSkinnedAnimation();
	}

	SkinnedConstants skinnedConstants = {};
	copy(skinnedModelInsts["Vanguard"]->finalTransforms_.begin(), skinnedModelInsts["Vanguard"]->finalTransforms_.end(),
		&skinnedConstants.boneTransforms_[0]);

	//for (int i = 0; i < 96; i++)
	//	XMStoreFloat4x4(&skinnedConstants.boneTransforms_[i], XMMatrixTranspose(XMLoadFloat4x4(&MathHelper::Identity4x4())));
	//XMStoreFloat4x4(&skinnedConstants.boneTransforms_[0], XMMatrixTranspose(XMMatrixTranslation(0.0f, 10.0f, 0.0f)));

	currSkinnedCB->CopyData(0, skinnedConstants);
}

void Renderer::UpdateMaterialBuffer(const GameTimer& gt, Scene* scene)
{
	auto currMaterialCB = currFrameResource_->materialBuffer_.get();
	auto& materials = scene->GetMaterials();
	for (auto& e : materials) {
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->numFramesDirty_ > 0) {
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->matTransform_);

			MaterialData matData;
			matData.diffuseAlbedo_ = mat->diffuseAlbedo_;
			matData.fresnelR0_ = mat->fresnelR0_;
			matData.roughness_ = mat->roughness_;
			XMStoreFloat4x4(&matData.matTransform_, XMMatrixTranspose(matTransform));
			matData.diffuseMapIndex_ = mat->diffuseSrvHeapIndex_;

            matData.atlasWidth_ = mat->width_;
            matData.atlasHeight_ = mat->height_;

			currMaterialCB->CopyData(mat->matCBIndex_, matData);

			// Next FrameResource need to be updated too.
			mat->numFramesDirty_--;
		}
	}
}

void Renderer::UpdateMainPassCB(const GameTimer& gt, Scene* scene)
{
    const Camera* mainCamera = scene->GetCamera();

	XMMATRIX view = mainCamera->GetView();
	XMMATRIX proj = mainCamera->GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mainPassCB_.view_, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mainPassCB_.invView_, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mainPassCB_.proj_, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mainPassCB_.invProj_, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mainPassCB_.viewProj_, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mainPassCB_.invViewProj_, XMMatrixTranspose(invViewProj));
	mainPassCB_.eyePosW_ = mainCamera->GetPosition3f();
	mainPassCB_.renderTargetSize_ = XMFLOAT2((float)clientWidth_, (float)clientHeight_);
	mainPassCB_.invRenderTargetSize_ = XMFLOAT2(1.0f / clientWidth_, 1.0f / clientHeight_);
	mainPassCB_.nearZ_ = mainCamera->GetNearZ();
	mainPassCB_.farZ_ = mainCamera->GetFarZ();
	mainPassCB_.totalTime_ = gt.TotalTime();
	mainPassCB_.deltaTime_ = gt.DeltaTime();
	mainPassCB_.ambientLight_ = { 0.25f, 0.25f, 0.35f, 1.0f };
	mainPassCB_.lights_[0].direction_ = { 0.57735f, -0.57735f, 0.57735f };
	mainPassCB_.lights_[0].strength_ = { 0.6f, 0.6f, 0.6f };
	mainPassCB_.lights_[1].direction_ = { -0.57735f, -0.57735f, 0.57735f };
	mainPassCB_.lights_[1].strength_ = { 0.3f, 0.3f, 0.3f };
	mainPassCB_.lights_[2].direction_ = { 0.0f, -0.707f, -0.707f };
	mainPassCB_.lights_[2].strength_ = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = currFrameResource_->passCB_.get();
	currPassCB->CopyData(0, mainPassCB_);
}

void Renderer::DrawRenderItems(const vector<RenderItem*>& ritems)
{
	uint32_t objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	uint32_t skinnedCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));
	
	auto objectCB = currFrameResource_->objectCB_->Resource();
	auto skinnedCB = currFrameResource_->skinnedCB_->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i) {
		auto ri = ritems[i];

		commandList_->IASetVertexBuffers(0, 1, &ri->mesh_->VertexBufferView());
		commandList_->IASetIndexBuffer(&ri->mesh_->IndexBufferView());
		commandList_->IASetPrimitiveTopology(ri->primitiveType_);
		/*
		CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart());
		texHandle.Offset(ri->material_->diffuseSrvHeapIndex_, cbvSrvDescriptorSize_);
		commandList_->SetGraphicsRootDescriptorTable(5, texHandle);
		*/
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->objCBIndex_ * objCBByteSize;
		commandList_->SetGraphicsRootConstantBufferView(0, objCBAddress);

		if (ri->skinnedModelInst_ != nullptr) {
			D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress() + ri->skinnedCBIndex_ * skinnedCBByteSize;
			commandList_->SetGraphicsRootConstantBufferView(1, skinnedCBAddress);
		}

		commandList_->DrawIndexedInstanced(ri->indexCount_, ri->instanceCount_, ri->baseIndex_, ri->baseVertex_, ri->instanceOffset_);
	}
}

void Renderer::DrawDebugBox()
{
	commandList_->IASetVertexBuffers(0, 1, &debugMesh_.VertexUploadBufferView());
	commandList_->IASetIndexBuffer(nullptr);
	commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	commandList_->DrawInstanced(countDebugVertices_, 1, 0, 0);
}

void Renderer::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (dxgiFactory_->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND) {
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i) {
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

void Renderer::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND) {
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, backBufferFormat_);

		ReleaseCom(output);

		++i;
	}
}

void Renderer::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;

	// nullptr을 전달하여 지원되는 디스플레이 모드 개수를 가져옵니다.
	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	// 실제 디스플레이 모드 목록을 가져옵니다.
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto& x : modeList) {
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
			L"\n";

		// 디버그 출력 창에 디스플레이 모드 정보를 출력합니다.
		::OutputDebugString(text.c_str());
	}
}