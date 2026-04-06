#pragma once
#include "KeyInputManager.h"
#include "GameTimer.h"
#include "GameObject.h"
#include "Player.h"
#include "ModelLoader.h"
#include "GeometryGenerator.h"
#include "PhysicsWorld.h"
#include "ObjectManager.h"
#include "Rigidbody.h"

class Scene
{
public:
    Scene();
    ~Scene();
    
    void InitScene(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, int clientWidth, int clientHeight);
    void OnResize(int clientWidth, int clientHeight);

    void KeyInput(const KeyInputManager& keyInput, float dt);
    void Update(const GameTimer& gt);

    Player* GetPlayer();
    Camera* GetCamera();

    const uint32_t GetNumInstances()const;

    const size_t MaxNumGameObjects()const;
    const vector<GameObject*>& GetAllGameObjects();
    const vector<GameObject*>& GetGameObjects(const spe::RigidbodyType layer);
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
    void BuildBillboardGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    void BuildMaterials();

    void BuildGameObjects();

    // build game object and add to game object manager
    GameObject* BuildGameObject(const XMFLOAT3& scale, const XMFLOAT3& rotate, const XMFLOAT3& transform,
        vector<RenderItem*>& rItems, spe::RigidbodyType rigidbodyType);

    // build render item and add to render item layer
    RenderItem* BuildRenderItem(const RenderLayer renderLayer,
        const MeshGeometry* mesh, const Submesh& submesh, const Material* material, 
        const XMMATRIX& world = XMMatrixIdentity(), const XMMATRIX& texTransform = XMMatrixIdentity());
    RenderItem* BuildSkinnedRenderItem(const RenderLayer renderLayer,
        const MeshGeometry* mesh, const Submesh& submesh, const Material* material,
        const XMMATRIX& world = XMMatrixIdentity(), const XMMATRIX& texTransform = XMMatrixIdentity(),
        SkinnedModelInstance* skinnedModelInstance = nullptr, const int32_t skinnedCBIndex = -1);
    RenderItem* BuildBillboardRenderItem(const RenderLayer renderLayer,
        const MeshGeometry* mesh, const Submesh& submesh, const Material* material,
        const XMMATRIX& world = XMMatrixIdentity(), const XMMATRIX& texTransform = XMMatrixIdentity(),
        const bool isBillboardYAxisFixed = true);

    GameObject* AddCylinderObject(const XMFLOAT3& pos, const XMFLOAT3& rotate = XMFLOAT3(0.f, 0.f, 0.f));
    GameObject* AddBoxObject(const XMFLOAT3& pos, const XMFLOAT3& rotate = XMFLOAT3(0.f, 0.f, 0.f));
    GameObject* AddBallObject(const XMFLOAT3& pos);

    void RemoveGameObject(GameObject* gameObject);
    void RemoveRenderItem(RenderItem* item, RenderLayer layer);

    void LoadScene(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    void LoadModels(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    void LoadTextures(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    void LoadTexture(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
        const string& name, const wstring& fileName);

    void AnimateMaterials(float dt);

    void Pick(int mouseX, int mouseY);
private:
    unique_ptr<Player> player_;

    Camera* mainCamera_ = nullptr;

    const uint32_t MAX_NUM_OBJECTS = 128;
    LayeredObjectManager<GameObject, spe::RigidbodyType> gameObejctManager_;

    vector<unique_ptr<RenderItem>> allRenderItems_;
    vector<RenderItem*> renderItemLayer_[static_cast<uint8_t>(RenderLayer::COUNT)];

    unordered_map<string, unique_ptr<MeshGeometry>> meshes_;
    unordered_map<string, unique_ptr<Material>> materials_;
    unordered_map<string, unique_ptr<Texture>> textures_;
    unordered_map<string, unique_ptr<SkinnedData>> skinnedData_;

    uint32_t numInstances_ = 0;

    unordered_map<string, unique_ptr<SkinnedModelInstance>> skinnedModelInsts_;

    spe::PhysicsWorld physicsWorld_;

    int clientWidth_, clientHeight_;
};