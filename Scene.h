#pragma once
#include "KeyInputManager.h"
#include "GameTimer.h"
#include "GameObject.h"
#include "Player.h"
#include "ModelLoader.h"
#include "GeometryGenerator.h"

class Scene
{
public:
    Scene();

    void InitScene(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

    void KeyInput(const KeyInputManager& keyInput, float dt);
    void AddObject();
    void Update(float dt);

    Player* GetPlayer();
    Camera* GetCamera();

    const vector<unique_ptr<GameObject>>& GetGameObjects()const;
    const vector<unique_ptr<RenderItem>>& GetAllRenderItems()const;
    const vector<RenderItem*>& GetRenderItems(RenderLayer layer) const;

    const unordered_map<string, unique_ptr<MeshGeometry>>& GetMeshes() const;
    const MeshGeometry* GetMesh(const string& name) const;
    const unordered_map<string, unique_ptr<Material>>& GetMaterials() const;
    const Material* GetMaterial(const string& name) const;
    const unordered_map<string, unique_ptr<Texture>>& GetTextures() const;
    const Texture* GetTexture(const string& name) const;
    const unordered_map<string, unique_ptr<SkinnedData>>& GetSkinnedData() const;
    const SkinnedData* GetSkinnedData(const string& name) const;
    unordered_map<string, unique_ptr<SkinnedModelInstance>>& GetSkinnedModelInsts();
    SkinnedModelInstance* GetSkinnedModelInst(const string& name);

private:

    void BuildScene(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    void BuildShapeGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    void BuildMaterials();
    void BuildRenderItems();
    RenderItem* BuildRenderItem(const uint8_t renderLayer,
        MeshGeometry* mesh, const Submesh& submesh, Material* material,
        const XMMATRIX& worldTransform = XMMatrixIdentity(),
        const XMMATRIX& texTransform = XMMatrixIdentity(),
        SkinnedModelInstance* skinnedModelInstance = nullptr, const int32_t skinnedCBIndex = -1);

    void LoadScene(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    void LoadModels(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    void LoadTextures(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    void LoadTexture(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
    const string& name, const wstring& fileName);

private:

    unique_ptr<Player> player_;

    Camera* mainCamera_ = nullptr;

    vector<unique_ptr<GameObject>> gameObjects_;

    vector<unique_ptr<RenderItem>> allRenderItems_;
    vector<RenderItem*> renderItemLayer_[(uint8_t)RenderLayer::Count];

    unordered_map<string, unique_ptr<MeshGeometry>> meshes_;
    unordered_map<string, unique_ptr<Material>> materials_;
    unordered_map<string, unique_ptr<Texture>> textures_;
    unordered_map<string, unique_ptr<SkinnedData>> skinnedData_;

    unordered_map<string, unique_ptr<SkinnedModelInstance>> skinnedModelInsts_;
};