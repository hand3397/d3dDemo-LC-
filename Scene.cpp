#include "stdafx.h"
#include "Scene.h"

void Scene::InitScene(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	player_ = make_unique<Player>("Player");
	mainCamera_ = player_.get()->GetCamera();

	LoadScene(device, cmdList);
	BuildScene(device, cmdList);
}

void Scene::KeyInput(const KeyInputManager& keyInput, float dt)
{

}

void Scene::AddObject()
{
}

void Scene::Update(float dt)
{

}

Player* Scene::GetPlayer()
{
    return player_.get();
}

Camera* Scene::GetCamera()
{
    return mainCamera_;
}

const vector<unique_ptr<GameObject>>& Scene::GetGameObjects() const
{
    return gameObjects_;
}

const vector<unique_ptr<RenderItem>>& Scene::GetAllRenderItems() const
{
    return allRenderItems_;
}

const vector<RenderItem*>& Scene::GetRenderItems(RenderLayer layer) const
{
    return renderItemLayer_[(uint8_t)layer];
}

const unordered_map<string, unique_ptr<MeshGeometry>>& Scene::GetMeshes() const
{
    return meshes_;
}

const MeshGeometry* Scene::GetMesh(const string& name) const
{
    auto it = meshes_.find(name);
    return (it != meshes_.end()) ? it->second.get() : nullptr;
}

const unordered_map<string, unique_ptr<Material>>& Scene::GetMaterials() const
{
    return materials_;
}

const Material* Scene::GetMaterial(const string& name) const
{
    auto it = materials_.find(name);
    return (it != materials_.end()) ? it->second.get() : nullptr;
}

const unordered_map<string, unique_ptr<Texture>>& Scene::GetTextures() const
{
    return textures_;
}

const Texture* Scene::GetTexture(const string& name) const
{
    auto it = textures_.find(name);
    return (it != textures_.end()) ? it->second.get() : nullptr;
}

const unordered_map<string, unique_ptr<SkinnedData>>& Scene::GetSkinnedData() const
{
    return skinnedData_;
}

const SkinnedData* Scene::GetSkinnedData(const string& name) const
{
    auto it = skinnedData_.find(name);
    return (it != skinnedData_.end()) ? it->second.get() : nullptr;
}

unordered_map<string, unique_ptr<SkinnedModelInstance>>& Scene::GetSkinnedModelInsts()
{
    return skinnedModelInsts_;
}

SkinnedModelInstance* Scene::GetSkinnedModelInst(const string& name)
{
    auto it = skinnedModelInsts_.find(name);
    return (it != skinnedModelInsts_.end()) ? it->second.get() : nullptr;
}

void Scene::BuildScene(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	BuildShapeGeometry(device, cmdList);
	BuildMaterials();
	BuildRenderItems();
}

void Scene::BuildShapeGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
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

	auto geo = make_unique<MeshGeometry>();
	geo->BuildMeshGeo("shapeGeo", vertices, indices, device, cmdList);
	geo->AddSubmesh("box", boxSubmesh);
	geo->AddSubmesh("grid", gridSubmesh);
	geo->AddSubmesh("sphere", sphereSubmesh);
	geo->AddSubmesh("cylinder", cylinderSubmesh);
	meshes_[geo->name_] = move(geo);
}

void Scene::BuildMaterials()
{
	materials_.insert({ "bricks0",	make_unique<Material>("bricks0", materials_.size(), 
		textures_["bricksTex"]->SrvHeapIndex, -1, 
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.02f, 0.02f, 0.02f), 0.1f) });

	materials_.insert({ "stone0",	make_unique<Material>("stone0", materials_.size(), 
		textures_["stoneTex"]->SrvHeapIndex, -1,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.3f) });

	materials_.insert({ "tile0",	make_unique<Material>("tile0", materials_.size(), 
		textures_["tileTex"]->SrvHeapIndex, -1,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.02f, 0.02f, 0.02f), 0.3f) });

	materials_.insert({ "ice0",		make_unique<Material>("ice0", materials_.size(), 
		textures_["iceTex"]->SrvHeapIndex, -1,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f), XMFLOAT3(0.1f, 0.1f, 0.1f), 0.0f) });

	materials_.insert({ "wirefence", make_unique<Material>("wirefence",	materials_.size(), 
		textures_["fenceTex"]->SrvHeapIndex, -1,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.1f, 0.1f, 0.1f), 0.25f) });

	materials_.insert({ "soldier",	make_unique<Material>("soldier", materials_.size(), 
		textures_["soldierTex"]->SrvHeapIndex, -1,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.1f, 0.1f, 0.1f), 0.25f) });
}

void Scene::BuildRenderItems()
{
	UINT objCBIndex = 0;

	BuildRenderItem((uint8_t)RenderLayer::AlphaTested,
		meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["box"], materials_["wirefence"].get(),
		XMMatrixScaling(3.0f, 3.0f, 3.0f) * XMMatrixTranslation(0.0f, 3.0f, 0.0f));

	BuildRenderItem((uint8_t)RenderLayer::Transparent,
		meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["box"], materials_["ice0"].get(),
		XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(0.0f, 4.0f, 0.0f));

	BuildRenderItem((uint8_t)RenderLayer::Opaque,
		meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["grid"], materials_["tile0"].get(),
		XMMatrixIdentity(), XMMatrixScaling(8.0f, 8.0f, 1.0f));

	auto demoBox = BuildRenderItem((uint8_t)RenderLayer::Opaque,
		meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["box"], materials_["soldier"].get(),
		XMMatrixIdentity(), XMMatrixIdentity());

	auto palyerRenderItem1 = BuildRenderItem((uint8_t)RenderLayer::Skinned,
		meshes_["skinnedGeo"].get(), meshes_["skinnedGeo"].get()->subMeshes_["0"], materials_["soldier"].get(),
		XMMatrixIdentity(), XMMatrixIdentity(),
		skinnedModelInsts_["Vanguard"].get(), 0);

	auto palyerRenderItem2 = BuildRenderItem((uint8_t)RenderLayer::Skinned,
		meshes_["skinnedGeo"].get(), meshes_["skinnedGeo"].get()->subMeshes_["1"], materials_["soldier"].get(),
		XMMatrixIdentity(), XMMatrixIdentity(),
		skinnedModelInsts_["Vanguard"].get(), 0);

	player_->AddRenderItem(palyerRenderItem1);
	player_->AddRenderItem(palyerRenderItem2);

	//---------------------------------------

	for (int i = 0; i < 5; ++i) {
		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

		BuildRenderItem((uint8_t)RenderLayer::Opaque,
			meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["cylinder"], materials_["bricks0"].get(),
			leftCylWorld);

		BuildRenderItem((uint8_t)RenderLayer::Opaque,
			meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["cylinder"], materials_["bricks0"].get(),
			rightCylWorld);

		BuildRenderItem((uint8_t)RenderLayer::Opaque,
			meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["sphere"], materials_["stone0"].get(),
			leftSphereWorld);

		BuildRenderItem((uint8_t)RenderLayer::Opaque,
			meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["sphere"], materials_["stone0"].get(),
			rightSphereWorld);
	}
}

RenderItem* Scene::BuildRenderItem(const uint8_t renderLayer, 
	MeshGeometry* mesh, const Submesh& submesh, Material* material, 
	const XMMATRIX& worldTransform, const XMMATRIX& texTransform, 
	SkinnedModelInstance* skinnedModelInstance, const int32_t skinnedCBIndex)
{
	auto renderItem = make_unique<RenderItem>();
	XMStoreFloat4x4(&renderItem->world_, worldTransform);
	XMStoreFloat4x4(&renderItem->texTransform_, texTransform);
	renderItem->objCBIndex_ = allRenderItems_.size();
	renderItem->material_ = material;
	renderItem->mesh_ = mesh;
	//renderItem->primitiveType_ = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	renderItem->indexCount_ = submesh.IndexCount;
	renderItem->startIndexLocation_ = submesh.StartIndexLocation;
	renderItem->baseVertexLocation_ = submesh.BaseVertexLocation;
	renderItem->skinnedModelInst_ = skinnedModelInstance;
	renderItem->skinnedCBIndex_ = skinnedCBIndex;

	RenderItem* rItem = renderItem.get();

	renderItemLayer_[renderLayer].push_back(rItem);
	allRenderItems_.push_back(move(renderItem));
	return rItem;
}

void Scene::LoadScene(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    LoadTextures(device, cmdList);
    LoadModels(device, cmdList);
}

void Scene::LoadModels(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    ModelLoader modelLoader;
    modelLoader.ReadModelFile("Models/Vanguard/Vanguard.fbx", 1.0f);
    modelLoader.ReadAnimationFile("Models/Vanguard/Animations/Idle.fbx", "Idle");
    modelLoader.ReadAnimationFile("Models/Vanguard/Animations/RunForward.fbx", "RunF");
    modelLoader.ReadAnimationFile("Models/Vanguard/Animations/WalkForward.fbx", "WalkF");
    modelLoader.ReadAnimationFile("Models/Vanguard/Animations/WalkBack.fbx", "WalkB");
    modelLoader.ReadAnimationFile("Models/Vanguard/Animations/StrafeLeft1.fbx", "WalkL1");
    modelLoader.ReadAnimationFile("Models/Vanguard/Animations/StrafeLeft2.fbx", "WalkL2");
    modelLoader.ReadAnimationFile("Models/Vanguard/Animations/StrafeRight1.fbx", "WalkR1");
    modelLoader.ReadAnimationFile("Models/Vanguard/Animations/StrafeRight2.fbx", "WalkR2");
    modelLoader.ReadAnimationFile("Models/Vanguard/Animations/Jump.fbx", "Jump");
    modelLoader.ReadAnimationFile("Models/Vanguard/Animations/Falling.fbx", "Falling");

    skinnedData_["Vanguard"] = make_unique<SkinnedData>(move(modelLoader.skinnedData_));
    auto skinnedData = skinnedData_["Vanguard"].get();
    skinnedModelInsts_["Vanguard"] = make_unique<SkinnedModelInstance>();
    auto skinnedModelInst = skinnedModelInsts_["Vanguard"].get();
    skinnedModelInst->skinnedInfo_ = skinnedData;
    skinnedModelInst->finalTransforms_.resize(skinnedData->BoneCount());

    skinnedData->AddBlendingAnimation("WalkFL", "WalkF", "WalkL1", 0.5f);
    skinnedData->AddBlendingAnimation("WalkFR", "WalkF", "WalkR1", 0.5f);
    skinnedData->AddBlendingAnimation("WalkBR", "WalkB", "WalkR2", 0.5f);
    skinnedData->AddBlendingAnimation("WalkBL", "WalkB", "WalkL2", 0.5f);

    const UINT vbByteSize = (UINT)modelLoader.skinnedMesh_.vertices.size() * sizeof(SkinnedVertex);
    const UINT ibByteSize = (UINT)modelLoader.skinnedMesh_.indices.size() * sizeof(uint32_t);

    auto geo = make_unique<MeshGeometry>();
    geo->BuildMeshGeo("skinnedGeo", modelLoader.skinnedMesh_.vertices, modelLoader.skinnedMesh_.indices,
        device, cmdList);
    geo->AddSubmesh("0", modelLoader.subMeshes_[0]);
    geo->AddSubmesh("1", modelLoader.subMeshes_[1]);
    meshes_[geo->name_] = move(geo);
}

void Scene::LoadTextures(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    vector<pair<string, wstring>> texNames = {
        {"bricksTex",	L"Textures/d3d12/bricks.dds"},
        {"stoneTex",	L"Textures/d3d12/stone.dds"},
        {"tileTex",		L"Textures/d3d12/tile.dds"},
        {"iceTex",		L"Textures/d3d12/ice.dds"},
        {"fenceTex",	L"Textures/d3d12/WireFence.dds"},
        {"soldierTex",	L"Textures/soldier.dds"},
    };

    for (auto& [name, fileName] : texNames)
        LoadTexture(device, cmdList, name, fileName);
}

void Scene::LoadTexture(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const string& name, const wstring& fileName)
{
    auto tex = make_unique<Texture>();
	tex->Name = name;
	tex->Filename = fileName;
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(device, cmdList, 
		tex->Filename.c_str(), tex->Resource, tex->UploadHeap));
	tex->SrvHeapIndex = textures_.size();

    textures_[tex->Name] = move(tex);
}

