#include "Scene.h"

Scene::Scene() : 
	gameObejctManager_(MAX_NUM_OBJECTS), physicsWorld_(spe::PhysicsWorld(this))
{
}

Scene::~Scene()
{
	allRenderItems_.clear();
	for (auto& layer : renderItemLayer_)
		layer.clear();
}

void Scene::InitScene(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, int clientWidth, int clientHeight)
{
	OnResize(clientWidth, clientHeight);

	player_ = make_unique<Player>(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 5.0f, -10.0f));
	mainCamera_ = player_->GetCamera();
	mainCamera_->RotatePitch(0.4f);

	LoadScene(device, cmdList);
	BuildScene(device, cmdList);

	// 현재 scene에 빌드된 rigidbody를 전부 등록한다.
	physicsWorld_.InitSceneObjects();
}

void Scene::OnResize(int clientWidth, int clientHeight)
{
	clientWidth_ = clientWidth;
	clientHeight_ = clientHeight;
}

void Scene::KeyInput(const KeyInputManager& keyInput, float dt)
{
	// 화면 클릭
	if (keyInput.WasMousePressed(MouseButton::LMB)) {
		int dx, dy;
		keyInput.GetMousePos(dx, dy);
		Pick(dx, dy);
	}
	
	if (player_)
		player_->KeyInput(keyInput, dt);	

	if (keyInput.WasKeyPressed('O') && player_) {
		auto ball = AddBallObject(player_->GetPosition());
		XMFLOAT3 newVelocity;
		XMStoreFloat3(&newVelocity, 50.0f * player_->GetLook());
		ball->GetRigidbody()->SetLinearVelocity(newVelocity);
	}
}

void Scene::Update(const GameTimer& gt)
{
	float dt = gt.DeltaTime();
	
	physicsWorld_.Update(dt);
	
	if (player_) {
		player_->GetRigidbody()->Integrate(dt);
		player_->Update(dt);
	}
		
	AnimateMaterials(dt);
}

Player* Scene::GetPlayer()
{
	return player_.get();
}

Camera* Scene::GetCamera()
{
	return mainCamera_;
}

const uint32_t Scene::GetNumInstances() const
{
	return numInstances_;
}

const size_t Scene::MaxNumGameObjects() const
{
	return MAX_NUM_OBJECTS;
}

const vector<GameObject*>& Scene::GetAllGameObjects()
{
	return gameObejctManager_.GetAllObjects();
}

const vector<GameObject*>& Scene::GetGameObjects(const spe::RigidbodyType layer)
{
	return gameObejctManager_.GetLayeredObjects(layer);
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
	BuildBillboardGeometry(device, cmdList);
    BuildStageGeometry(device, cmdList);
	BuildMaterials();
	BuildGameObjects();
}

void Scene::BuildShapeGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(2.0f, 2.0f, 2.0f, 3);
	GeometryGenerator::MeshData wall = geoGen.CreateOnBox(1.0f, 3.0f, 0.1f, 1);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(150.0f, 150.0f, 150, 150);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(1.0f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(1.0f, 1.0f, 2.0f, 20, 5);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT wallVertexOffset = (UINT)box.Vertices.size();
	UINT gridVertexOffset = wallVertexOffset + (UINT)wall.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT wallIndexOffset = (UINT)box.Indices32.size();
	UINT gridIndexOffset = wallIndexOffset + (UINT)wall.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	// Define the SubmeshGeometry that cover different 
	// regions of the vertex/index buffers.
	BoundingBox bb;
	BoundingSphere bs;

	bb.Center = MathHelper::GetCenterFloat3(box.minPos, box.maxPos);
	bb.Extents = MathHelper::GetExtentsFloat3(box.minPos, box.maxPos);
	bs.Center = bb.Center;
	bs.Radius = XMVectorGetX(XMVector3Length(XMLoadFloat3(&bb.Extents)));
	Submesh boxSubmesh((UINT)box.Indices32.size(), boxIndexOffset, boxVertexOffset, bb, bs);

	bb.Center = MathHelper::GetCenterFloat3(wall.minPos, wall.maxPos);
	bb.Extents = MathHelper::GetExtentsFloat3(wall.minPos, wall.maxPos);
	bs.Center = bb.Center;
	bs.Radius = XMVectorGetX(XMVector3Length(XMLoadFloat3(&bb.Extents)));
	Submesh wallSubmesh((UINT)wall.Indices32.size(), wallIndexOffset, wallVertexOffset, bb, bs);
	
	bb.Center = MathHelper::GetCenterFloat3(grid.minPos, grid.maxPos);
	bb.Extents = MathHelper::GetExtentsFloat3(grid.minPos, grid.maxPos);
	bs.Center = bb.Center;
	bs.Radius = XMVectorGetX(XMVector3Length(XMLoadFloat3(&bb.Extents)));
	Submesh gridSubmesh((UINT)grid.Indices32.size(), gridIndexOffset, gridVertexOffset, bb, bs);
	
	bb.Center = MathHelper::GetCenterFloat3(sphere.minPos, sphere.maxPos);
	bb.Extents = MathHelper::GetExtentsFloat3(sphere.minPos, sphere.maxPos);
	bs.Center = bb.Center;
	bs.Radius = XMVectorGetX(XMVector3Length(XMLoadFloat3(&bb.Extents)));
	Submesh sphereSubmesh((UINT)sphere.Indices32.size(), sphereIndexOffset, sphereVertexOffset, bb, bs);
	
	bb.Center = MathHelper::GetCenterFloat3(cylinder.minPos, cylinder.maxPos);
	bb.Extents = MathHelper::GetExtentsFloat3(cylinder.minPos, cylinder.maxPos);
	bs.Center = bb.Center;
	bs.Radius = XMVectorGetX(XMVector3Length(XMLoadFloat3(&bb.Extents)));
	Submesh cylinderSubmesh((UINT)cylinder.Indices32.size(), cylinderIndexOffset, cylinderVertexOffset, bb, bs);
	
	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		wall.Vertices.size() +
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

	for (size_t i = 0; i < wall.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = wall.Vertices[i].Position;
		vertices[k].Normal = wall.Vertices[i].Normal;
		vertices[k].TexC = wall.Vertices[i].TexC;
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
	indices.insert(indices.end(), begin(wall.Indices32), end(wall.Indices32));
	indices.insert(indices.end(), begin(grid.Indices32), end(grid.Indices32));
	indices.insert(indices.end(), begin(sphere.Indices32), end(sphere.Indices32));
	indices.insert(indices.end(), begin(cylinder.Indices32), end(cylinder.Indices32));

	auto geo = make_unique<MeshGeometry>();
	geo->BuildMeshGeo("shapeGeo", vertices, indices, device, cmdList);
	geo->AddSubmesh("box", boxSubmesh);
	geo->AddSubmesh("wall", wallSubmesh);
	geo->AddSubmesh("grid", gridSubmesh);
	geo->AddSubmesh("sphere", sphereSubmesh);
	geo->AddSubmesh("cylinder", cylinderSubmesh);
	meshes_[geo->name_] = move(geo);
}

void Scene::BuildBillboardGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	struct BillboardGeometryDesc
	{
		string name;
		XMFLOAT3 center;
		XMFLOAT2 size;
	};

	BoundingBox bb;
	BoundingSphere bs;

	const vector<BillboardGeometryDesc> billboardGeometries = {
		{"test",		XMFLOAT3(0.f, 0.f, 0.f),	XMFLOAT2(1.0f, 1.0f)},
		{"character0",	XMFLOAT3(0.f, 0.2f, 0.f),	XMFLOAT2(1.0f, 1.0f)},
		{"character1",	XMFLOAT3(0.f, 0.75f, 0.f),	XMFLOAT2(1.0f, 1.5f)},
		{"tree3",		XMFLOAT3(0.f, 7.0f, 0.f),	XMFLOAT2(5.0f, 15.0f)},
	};

	uint32_t numBillboardGeometries = billboardGeometries.size();
	vector<BillboardVertex> vertices(numBillboardGeometries);
	vector<uint32_t> indices(numBillboardGeometries);

	for (int i = 0; i < numBillboardGeometries; ++i) {
		const BillboardGeometryDesc& billboard = billboardGeometries[i];
		vertices[i].Pos = billboard.center;
		vertices[i].Size = billboard.size;
		indices[i] = i;
	}

	auto geo = make_unique<MeshGeometry>();
	geo->BuildMeshGeo("billboardGeo", vertices, indices, device, cmdList);

	for (int i = 0; i < numBillboardGeometries; ++i) {
		XMFLOAT3 center = vertices[i].Pos;
		XMFLOAT2 size = vertices[i].Size;

		bb.Center = center;
		bb.Extents = { size.x, size.y, size.x };
		bs.Center = center;
		bs.Radius = sqrtf(size.x * size.x + size.y * size.y);

		Submesh billboardSubmesh(1, i, 0, bb, bs);
		geo->AddSubmesh(billboardGeometries[i].name, billboardSubmesh);
	}
	
	meshes_[geo->name_] = move(geo);
}

void Scene::BuildStageGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    const float tileSize = stage_.GetTileSize();
    const float tileHeight = stage_.GetTileHeight();

	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(tileSize, tileHeight, tileSize, 0);
	
	// -z +z +y -y -x +x
	const int direction[6][3] = {
		{ 0, 0, -1 }, { 0, 0, 1 }, { 0, 1, 0 },
		{ 0, -1, 0 }, { -1, 0, 0 }, { 1, 0, 0 }
    };
	const int vertex[6][4] = {
		{0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11},
        {12, 13, 14, 15}, {16, 17, 18, 19}, {20, 21, 22, 23}
	};
    // 각 면을 이루는 vertex의 index (GeometryGenerator의 면 생성 방식대로 따라감)
	int index[6] = {0, 1, 2, 0, 2, 3};

	const uint32_t stageWidth = stage_.GetStageWidth();
	const uint32_t stageLength = stage_.GetStageLength();
	const uint32_t stageHeight = stage_.GetStageHeight();

	vector<VertexTexArray> vertices;
	vector<uint32_t> indices;

    // stage가 world의 중심에 위치하도록 vertex의 offset을 계산한다. (바닥은 y = 0에 맞춘다)
	XMFLOAT3 vertexOffset = { -tileSize * 0.5f * stageWidth, tileHeight * 0.5f, -tileSize * 0.5f * stageLength };

	for (int x = 0; x < stageWidth; ++x) {
		for (int y = 0; y < stageHeight; ++y) {
			for (int z = 0; z < stageLength; ++z) {
                // x, y, z 위치에 블록이 존재하는지 확인한다. 만약 존재한다면, 해당 위치에 box를 그린다.
				for (int dir = 0; dir < 6; ++dir) {
					int dx = x + direction[dir][0];
					int dy = y + direction[dir][1];
					int dz = z + direction[dir][2];

                    // 현재 블록의 인접한 블록이 solid인지 확인한다. 만약 solid하다면, 현재 블록의 해당 방향 면을 그린다.
					if (!stage_.IsBlockSolid(dx, dy, dz)) {
						uint32_t numVertices = vertices.size();
						uint32_t numIndices = indices.size();
						vertices.reserve(numVertices + 4);
                        indices.reserve(numIndices + 6);

						for (int vi = 0; vi < 4; ++vi) {
                            const auto& v = box.Vertices[vertex[dir][vi]];
                            XMFLOAT3 dPos = { 
								v.Position.x + x * tileSize + vertexOffset.x, 
								v.Position.y + y * tileHeight + vertexOffset.y, 
								v.Position.z + z * tileSize + vertexOffset.z };
                            vertices.emplace_back(dPos, v.Normal, v.TexC, 1);
						}
						for (int ii = 0; ii < 6; ++ii) {
							indices.emplace_back(index[ii] + numVertices);
						}
					}
				}
			}
		}
	}

	const float widthHalf = tileSize * 0.5f * stageWidth;
	const float lengthHalf = tileSize * 0.5f * stageLength;
	const float heightHalf = tileHeight * 0.5f * stageHeight;
    const XMFLOAT3 terrainCenter = { 0.f, heightHalf, 0.f };
	
	Submesh terrainSubmesh((uint32_t)indices.size(), 0, 0);
	terrainSubmesh.boundingBox_ = BoundingBox(terrainCenter,
		XMFLOAT3(widthHalf, heightHalf, lengthHalf));
	terrainSubmesh.boundingSphere_ = BoundingSphere(terrainCenter, 
		sqrtf(widthHalf * widthHalf + heightHalf * heightHalf + lengthHalf * lengthHalf));

	auto geo = make_unique<MeshGeometry>();
	geo->BuildMeshGeo("terrainGeo", vertices, indices, device, cmdList);
	geo->AddSubmesh("terrain", terrainSubmesh);
	meshes_[geo->name_] = move(geo);
}

void Scene::BuildMaterial(const string& name, const string& textureName, const XMFLOAT4& diffuseAlbedo, 
	const XMFLOAT3& fresnelR0, float roughness, const XMMATRIX& matTransform)
{
    if (textures_.find(textureName) == textures_.end()) {
        // Handle the case where the texture is not found
        return;
    }

    const auto& textrue = textures_[textureName];

    materials_.insert({ name, 
		make_unique<Material>(name, materials_.size(), textrue->srvHeapIndex_, textrue->normalSrvHeapIndex_,
			diffuseAlbedo, fresnelR0, roughness, textrue->width_, textrue->height_, matTransform) });
}

void Scene::BuildMaterials()
{
	materials_.clear();

	BuildMaterial("bricks0",	"bricksTex",		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.02f, 0.02f, 0.02f),	0.1f);
	BuildMaterial("stone0",		"stoneTex",			XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f),	0.3f);
	BuildMaterial("tile0",		"tileTex",			XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.02f, 0.02f, 0.02f),	0.3f);
	BuildMaterial("ice0",		"iceTex",			XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f), XMFLOAT3(0.1f, 0.1f, 0.1f),		0.0f);
	BuildMaterial("wirefence",	"fenceTex",			XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.1f, 0.1f, 0.1f),		0.25f);
	BuildMaterial("soldier",	"soldierTex",		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.1f, 0.1f, 0.1f),		0.25f);
	BuildMaterial("knight",		"knightTex",		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f),		1.0f);
	BuildMaterial("tree",		"treeTex",			XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f),		1.0f);
	BuildMaterial("terrain",	"terrainTexArray",	XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f),		1.0f);

	/*
	for (auto& pair : textures_) {
		cout<< "Texture Name: " << pair.first << ", Index: " << pair.second->srvHeapIndex_ <<'\n';
	}

	for (auto& pair : materials_) {
        cout << "Material Name: " << pair.first << ", Index: " << pair.second->matCBIndex_ << '\n';
	}
	*/
}

void Scene::BuildGameObjects()
{
	vector<RenderItem*> renderItems;
	/*
	renderItems = { BuildRenderItem((uint8_t)RenderLayer::AlphaTested,
		meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["box"], materials_["wirefence"].get(),
		XMMatrixScaling(3.0f, 3.0f, 3.0f) * XMMatrixTranslation(0.0f, 3.0f, 0.0f), XMMatrixIdentity()) };
	BuildGameObject(XMFLOAT3(3.0f, 3.0f, 3.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 3.0f, 0.0f), 
		renderItems, RigidbodyType::Static);
	
	renderItems = { BuildRenderItem((uint8_t)RenderLayer::Transparent,
		meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["box"], materials_["ice0"].get(),
		XMMatrixTranslation(0.0f, 4.0f, 0.0f), XMMatrixIdentity()) };
	BuildGameObject(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 4.0f, 0.0f),
		renderItems, RigidbodyType::Static);
	*/
	renderItems = { BuildRenderItem(RenderLayer::RENDER_OPAQUE,
		meshes_["shapeGeo"].get(), meshes_["shapeGeo"]->subMeshes_["grid"], materials_["tile0"].get(),
		XMMatrixIdentity(), XMMatrixScaling(50.0f, 50.0f, 1.0f)) };
	GameObject* go = BuildGameObject(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f),
		renderItems, spe::RigidbodyType::STATIC);
	go->GetRigidbody()->GetFixture()->SetFriction(0.5);

	BuildRenderItem(RenderLayer::RENDER_TEX_ARRAY_OPAQUE,
		meshes_["terrainGeo"].get(), meshes_["terrainGeo"]->subMeshes_["terrain"], materials_["terrain"].get());

	BuildBillboardRenderItem(RenderLayer::RENDER_ALPHATESTED_BILLBOARD,
		meshes_["billboardGeo"].get(), meshes_["billboardGeo"]->subMeshes_["character0"], materials_["knight"].get(),
		XMMatrixTranslation(0.f, 0.f, 20.f), XMMatrixIdentity(), true);
	
	// build wall
	/*
	renderItems = { BuildRenderItem((uint8_t)RenderLayer::Opaque,
		meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["box"], materials_["soldier"].get(),
		XMMatrixIdentity(), XMMatrixIdentity()) };
	BuildGameObject(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f),
		renderItems, RigidbodyType::Static);
	
	auto wall = BuildRenderItem((uint8_t)RenderLayer::Instance,
		meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["wall"], materials_["stone0"].get(),
		XMMatrixIdentity(), XMMatrixIdentity());
	renderItems = { wall };
	const int n = 5;
	wall->instanceCount_ = n * n * n;
	numInstances += wall->instanceCount_;
	wall->instanceOffset_ = 0;
	wall->instances_.resize(numInstances);
	
	for (int x = 0; x < n; x++) {
		for (int y = 0; y < n; y++) {
			for (int z = 0; z < n; z++) {
				int index = x * n * n + y * n + z;
				XMStoreFloat4x4(&wall->instances_[index].World,
					XMMatrixTranslation(3.0f * x - 6.f, y * 4.0f, -2.0f * z));
				wall->instances_[index].MaterialIndex = index % materials_.size();
				BuildGameObject(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(3.0f * x - 6.f, y * 4.0f, -2.0f * z),
					renderItems, RigidbodyType::Static);
			}
		}
	}
	*/
	
	renderItems = { BuildSkinnedRenderItem(RenderLayer::RENDER_SKINNED,
		meshes_["skinnedGeo"].get(), meshes_["skinnedGeo"].get()->subMeshes_["0"], materials_["soldier"].get(),
		XMMatrixIdentity(), XMMatrixIdentity(),
		skinnedModelInsts_["Vanguard"].get(), 0),

		BuildSkinnedRenderItem(RenderLayer::RENDER_SKINNED,
		meshes_["skinnedGeo"].get(), meshes_["skinnedGeo"].get()->subMeshes_["1"], materials_["soldier"].get(),
		XMMatrixIdentity(), XMMatrixIdentity(),
		skinnedModelInsts_["Vanguard"].get(), 0) };
	player_->SetRenderItems(renderItems);
	
	/*
	for (int i = 0; i < 5; ++i) {
		XMFLOAT3 leftCylPos = XMFLOAT3(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMFLOAT3 rightCylPos = XMFLOAT3(+5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMFLOAT3 leftSpherePos = XMFLOAT3(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMFLOAT3 rightSpherePos = XMFLOAT3(+5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

		renderItems = { BuildRenderItem((uint8_t)RenderLayer::Opaque,
			meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["cylinder"], materials_["bricks0"].get(),
			leftCylWorld, XMMatrixIdentity()) };
		BuildGameObject(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), leftCylPos,
			renderItems, RigidbodyType::Static);

		renderItems = { BuildRenderItem((uint8_t)RenderLayer::Opaque,
			meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["cylinder"], materials_["bricks0"].get(),
			rightCylWorld, XMMatrixIdentity()) };
		BuildGameObject(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), rightCylPos,
			renderItems, RigidbodyType::Static);

		renderItems = { BuildRenderItem((uint8_t)RenderLayer::Opaque,
			meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["sphere"], materials_["stone0"].get(),
			leftSphereWorld, XMMatrixIdentity()) };
		BuildGameObject(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), leftSpherePos,
			renderItems, RigidbodyType::Static);

		renderItems = { BuildRenderItem((uint8_t)RenderLayer::Opaque,
			meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["sphere"], materials_["stone0"].get(),
			rightSphereWorld, XMMatrixIdentity()) };
		BuildGameObject(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), rightSpherePos,
			renderItems, RigidbodyType::Static);
	}
	*/

	AddBoxObject(XMFLOAT3(-5.0f, 1.0f, 0.f));
	AddBoxObject(XMFLOAT3(-5.3f, 3.0f, 0.f));
	AddBoxObject(XMFLOAT3(-5.6f, 5.0f, 0.f));

}

GameObject* Scene::BuildGameObject(const XMFLOAT3& scale, const XMFLOAT3& rotate, const XMFLOAT3& transform, 
	vector<RenderItem*>& rItems, spe::RigidbodyType rigidbodyType)
{
	XMFLOAT4 rotateQuat;
	XMStoreFloat4(&rotateQuat, XMQuaternionRotationRollPitchYaw(rotate.x, rotate.y, rotate.z));
	
	XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
	XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&rotateQuat));
	XMMATRIX T = XMMatrixTranslation(transform.x, transform.y, transform.z);
	XMMATRIX world = S * R * T;
	
	GameObject* gameObject = gameObejctManager_.CreateObject(scale, rotate, transform, rigidbodyType);
	
	auto rigidbody = gameObject->GetRigidbody();
	for (RenderItem* ri : rItems) {
		if (ri == nullptr) continue;

		XMStoreFloat4x4(&ri->world_, world);
		gameObject->AddRenderItem(ri);

		spe::BoxShape* box = new spe::BoxShape(ri->boundingBox_.Center, ri->boundingBox_.Extents);
		// box의 center는 local 좌표
		spe::Fixture* fixture = new spe::Fixture(box);
		fixture->SetFriction(0.4f);
		fixture->SetRestitution(0.4f);
		rigidbody->AddFixture(fixture);
	}
	
	return gameObject;
}

RenderItem* Scene::BuildRenderItem(const RenderLayer renderLayer, 
	const MeshGeometry* mesh, const Submesh& submesh, const Material* material, 
	const XMMATRIX& world, const XMMATRIX& texTransform)
{
	auto renderItem = make_unique<RenderItem>(renderLayer, mesh, submesh, material, world, texTransform);
	renderItem->objCBIndex_ = allRenderItems_.size();

	renderItem->boundingBox_ = submesh.boundingBox_;
	renderItem->boundingSphere_ = submesh.boundingSphere_;
	RenderItem* rItem = renderItem.get();
	renderItemLayer_[static_cast<uint8_t>(renderLayer)].push_back(rItem);
	allRenderItems_.push_back(move(renderItem));
	return rItem;
}

RenderItem* Scene::BuildSkinnedRenderItem(const RenderLayer renderLayer, 
	const MeshGeometry* mesh, const Submesh& submesh, const Material* material, 
	const XMMATRIX& world, const XMMATRIX& texTransform, 
	SkinnedModelInstance* skinnedModelInstance, const int32_t skinnedCBIndex)
{
	auto renderItem = make_unique<RenderItem>(renderLayer, mesh, submesh, material, world, texTransform);
	renderItem->skinnedModelInst_ = skinnedModelInstance;
	renderItem->skinnedCBIndex_ = skinnedCBIndex;
	renderItem->objCBIndex_ = allRenderItems_.size();

	renderItem->boundingBox_ = submesh.boundingBox_;
	renderItem->boundingSphere_ = submesh.boundingSphere_;
	RenderItem* rItem = renderItem.get();
	renderItemLayer_[static_cast<uint8_t>(renderLayer)].push_back(rItem);
	allRenderItems_.push_back(move(renderItem));
	return rItem;
}

RenderItem* Scene::BuildBillboardRenderItem(const RenderLayer renderLayer, 
	const MeshGeometry* mesh, const Submesh& submesh, const Material* material, 
	const XMMATRIX& world, const XMMATRIX& texTransform, const bool isBillboardYAxisFixed)
{
	auto renderItem = make_unique<RenderItem>(renderLayer, mesh, submesh, material, world, texTransform);
	
	renderItem->primitiveType_ = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
    renderItem->isBillboardYAxisFixed_ = isBillboardYAxisFixed;

	renderItem->objCBIndex_ = allRenderItems_.size();

	renderItem->boundingBox_ = submesh.boundingBox_;
	renderItem->boundingSphere_ = submesh.boundingSphere_;
	RenderItem* rItem = renderItem.get();
	renderItemLayer_[static_cast<uint8_t>(renderLayer)].push_back(rItem);
	allRenderItems_.push_back(move(renderItem));
	return rItem;
}

GameObject* Scene::AddCylinderObject(const XMFLOAT3& pos, const XMFLOAT3& rotate)
{
	XMMATRIX R = XMMatrixRotationRollPitchYaw(rotate.x, rotate.y, rotate.z);
	XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);
	XMMATRIX world = R * T;

	GameObject* gameObject = gameObejctManager_.CreateObject(
		XMFLOAT3(1.0f, 1.0f, 1.0f), rotate, pos, spe::RigidbodyType::DYNAMIC);

	auto renderItems = { BuildRenderItem(RenderLayer::RENDER_OPAQUE,
			meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["cylinder"], materials_["stone0"].get(),
			XMMatrixIdentity(), XMMatrixIdentity()) };

	for (RenderItem* ri : renderItems) {
		if (!ri) continue;
		//XMStoreFloat4x4(&ri->world_, world);
		gameObject->AddRenderItem(ri);
	}

	spe::Rigidbody* rigidbody = gameObject->GetRigidbody();
	rigidbody->SetMass(10.0f);

	spe::CylinderShape* cylinderShape = new spe::CylinderShape(XMFLOAT3(0.f, 0.f, 0.f), 1.0f, 2.0f);

	spe::Fixture* fixture = new spe::Fixture(cylinderShape);
	fixture->SetFriction(0.4f);
	fixture->SetRestitution(0.4f);
	rigidbody->AddFixture(fixture);
	rigidbody->ComputeInertiaTensor();

	return gameObject;
}

GameObject* Scene::AddBoxObject(const XMFLOAT3& pos, const XMFLOAT3& rotate)
{
	XMMATRIX R = XMMatrixRotationRollPitchYaw(rotate.x, rotate.y, rotate.z);
	XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);
	XMMATRIX world = R * T;

	GameObject* gameObject = gameObejctManager_.CreateObject(
		XMFLOAT3(1.0f, 1.0f, 1.0f), rotate, pos, spe::RigidbodyType::DYNAMIC);

	auto renderItems = { BuildRenderItem(RenderLayer::RENDER_OPAQUE,
			meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["box"], materials_["stone0"].get(),
			XMMatrixIdentity(), XMMatrixIdentity()) };

	for (RenderItem* ri : renderItems) {
		if (!ri) continue;
		//XMStoreFloat4x4(&ri->world_, world);
		gameObject->AddRenderItem(ri);
	}

	spe::Rigidbody* rigidbody = gameObject->GetRigidbody();
	rigidbody->SetMass(10.0f);

	spe::BoxShape* boxShape = new spe::BoxShape(XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(1.0f, 1.0f, 1.0f));

	spe::Fixture* fixture = new spe::Fixture(boxShape);
	fixture->SetFriction(0.4f);
	fixture->SetRestitution(0.4f);
	rigidbody->AddFixture(fixture);
	rigidbody->ComputeInertiaTensor();

	return gameObject;
}

GameObject* Scene::AddBallObject(const XMFLOAT3& pos)
{
	XMFLOAT4 rotateQuat = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);
	XMMATRIX world = T;

	GameObject* gameObject = gameObejctManager_.CreateObject(
		XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), pos, spe::RigidbodyType::DYNAMIC);
	
	auto renderItems = { BuildRenderItem(RenderLayer::RENDER_OPAQUE,
			meshes_["shapeGeo"].get(), meshes_["shapeGeo"].get()->subMeshes_["sphere"], materials_["bricks0"].get(),
			XMMatrixIdentity(), XMMatrixIdentity()) };

	for (RenderItem* ri : renderItems) {
		if (!ri) continue;
		XMStoreFloat4x4(&ri->world_, world);
		gameObject->AddRenderItem(ri);
	}

	auto rigidbody = gameObject->GetRigidbody();
	rigidbody->SetMass(7.0f);

	spe::SphereShape* sphereShape = new spe::SphereShape(XMFLOAT3(0.f, 0.f, 0.f), 1.0f);

	spe::Fixture* fixture = new spe::Fixture(sphereShape);
	fixture->SetFriction(0.3); 
	fixture->SetRestitution(0.4f);
	rigidbody->AddFixture(fixture);
	rigidbody->ComputeInertiaTensor();

	return gameObject;
}

void Scene::RemoveGameObject(GameObject* gameObject)
{
	if (gameObejctManager_.IsEmpty() || gameObject == nullptr)
		return;

	// 실제 rigidbody 제거는 gameObject가 제거될 때 이루어짐
	physicsWorld_.RemoveRigidbody(gameObject->GetRigidbody());
	const auto& renderItems = gameObject->GetRenderItems();
	for (RenderItem* rItem : renderItems) {
        if (rItem == nullptr) continue;
		RemoveRenderItem(rItem, rItem->renderLayer_);
	}

	gameObejctManager_.DestroyObject(gameObject);
}

void Scene::RemoveRenderItem(RenderItem* item, RenderLayer layer)
{
	if (item == nullptr)
        return;

	// 1. 레이어 벡터에서 제거
	auto& layerVec = renderItemLayer_[static_cast<uint8_t>(layer)];
	layerVec.erase(std::remove(layerVec.begin(), layerVec.end(), item), layerVec.end());

	// 2. 전체 관리 벡터에서 제거 (실제 메모리 해제)
	allRenderItems_.erase(std::remove_if(allRenderItems_.begin(), allRenderItems_.end(),
		[&](const unique_ptr<RenderItem>& ptr) { return ptr.get() == item; }), allRenderItems_.end());
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
    textures_.clear();

    // Texture name, file path, atlasWidth, atlasHeight	(atlasTexture가 아닐 경우 1, 1로 설정)
	vector<tuple<string, wstring, uint16_t, uint16_t, bool>> texNames = {
		{"bricksTex",	L"Textures/d3d12/bricks.dds",		1, 1, false},
		{"stoneTex",	L"Textures/d3d12/stone.dds",		1, 1, false},
		{"tileTex",		L"Textures/d3d12/tile.dds",			1, 1, false},
		{"iceTex",		L"Textures/d3d12/ice.dds",			1, 1, false},
		{"fenceTex",	L"Textures/d3d12/WireFence.dds",	1, 1, false},
		{"soldierTex",	L"Textures/soldier.dds",			1, 1, false},
		{"knightTex",	L"Textures/KnightAtlas.dds",		6, 8, false},
		{"treeTex",		L"Textures/d3d12/tree01S.dds",		1, 1, false},

        // terrainTexArray는 atlasTexture가 아니라 texture array이므로 isTexArray를 true로 설정한다. 
		// (LoadTexture에서 텍스처 배열로 로드하기 위해)
        // srvHeapIndex를 가장 크게 만들기 위해 texNames 벡터의 마지막에 위치시킨다.
		{"terrainTexArray", L"Textures/TerrainArray.dds",	1, 1, true}
	};
	
	for (auto& [name, fileName, width, height, isTexArray] : texNames)
		LoadTexture(device, cmdList, name, fileName, width, height, isTexArray);
}

void Scene::LoadTexture(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, 
	const string& name, const wstring& fileName, uint16_t atlasWidth , uint16_t atlasHeight, bool isTexArray)
{
	unique_ptr<Texture>& tex = make_unique<Texture>();

	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(device, cmdList,
		fileName.c_str(), tex->resource_, tex->uploadHeap_));
	
	if (tex->resource_) {
		tex->name_ = name;
		tex->fileName_ = fileName;
		tex->srvHeapIndex_ = textures_.size();
		tex->normalSrvHeapIndex_ = -1;
        tex->width_ = atlasWidth;
        tex->height_ = atlasHeight;
		textures_[tex->name_] = move(tex);
	}
}

void Scene::AnimateMaterials(float dt)
{
}

void Scene::Pick(int mouseX, int mouseY)
{
	if (mainCamera_ != nullptr) {
		const spe::Ray ray = mainCamera_->GetPickingRay(mouseX, mouseY, clientWidth_, clientHeight_);
		spe::Rigidbody* rigidbody = physicsWorld_.RayCast(ray);

		if (rigidbody != nullptr) {
			if (rigidbody->GetType() == spe::RigidbodyType::STATIC)
				return;
			
			RemoveGameObject(rigidbody->GetGameObject());
		}
	}
}

