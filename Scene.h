#pragma once
#include "KeyInputManager.h"
#include "GameTimer.h"
#include "GameObject.h"
#include "Player.h"
#include "ModelLoader.h"
#include "GeometryGenerator.h"
#include "GamePhysics.h"
#include "ObjectManager.h"

class Scene
{
public:
    Scene();
    ~Scene();
    
    void InitScene(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

    void KeyInput(const KeyInputManager& keyInput, float dt);
    void Update(const GameTimer& gt);

    Player* GetPlayer();
    Camera* GetCamera();

    const uint32_t GetNumInstances()const;

    const size_t MaxNumGameObjects()const;
    const vector<GameObject*>& GetAllGameObjects();
    const vector<GameObject*>& GetGameObjects(const RigidbodyType layer);
    const vector<unique_ptr<RenderItem>>& GetAllRenderItems()const;
    const vector<RenderItem*>& GetRenderItems(const RenderLayer layer) const;

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

    void BuildGameObjects();
    GameObject* BuildGameObject(const XMFLOAT3& scale, const XMFLOAT3& rotate, const XMFLOAT3& transform,
        vector<RenderItem*>& rItems, RigidbodyType rigidbodyType);

    RenderItem* BuildRenderItem(const uint8_t renderLayer, 
        const MeshGeometry* mesh, const Submesh& submesh, const Material* material, 
        const XMMATRIX& world = XMMatrixIdentity(), const XMMATRIX& texTransform = XMMatrixIdentity(),
        SkinnedModelInstance* skinnedModelInstance = nullptr, const int32_t skinnedCBIndex = -1);
    GameObject* AddBallObject(const XMFLOAT3& pos);

    void LoadScene(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    void LoadModels(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    void LoadTextures(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    void LoadTexture(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
        const string& name, const wstring& fileName);

    void AnimateMaterials(float dt);

private:
    unique_ptr<Player> player_;

    Camera* mainCamera_ = nullptr;

    const uint32_t MAX_NUM_OBJECTS = 128;
    LayeredObjectManager<GameObject, RigidbodyType> gameObejctManager_;

    vector<unique_ptr<RenderItem>> allRenderItems_;
    vector<RenderItem*> renderItemLayer_[(uint8_t)RenderLayer::Count];

    unordered_map<string, unique_ptr<MeshGeometry>> meshes_;
    unordered_map<string, unique_ptr<Material>> materials_;
    unordered_map<string, unique_ptr<Texture>> textures_;
    unordered_map<string, unique_ptr<SkinnedData>> skinnedData_;

    uint32_t numInstances = 0;

    unordered_map<string, unique_ptr<SkinnedModelInstance>> skinnedModelInsts_;

    GamePhysics gamePhysics;
};