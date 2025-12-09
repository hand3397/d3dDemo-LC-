#pragma once

#include "d3dUtil.h"
#include "RenderItem.h"
#include "Rigidbody.h"

using namespace std;
using namespace DirectX;

class GameObject
{
public:
    GameObject(const spe::RigidbodyType rigidbodyType);
    GameObject(const XMFLOAT3& scale, const XMFLOAT3& rotate, const XMFLOAT3& position, 
        const spe::RigidbodyType rigidbodyType);
    virtual ~GameObject() {}

    void OnSpawn();
    void OnDestroy();

    bool isActive() const;

    virtual void Update(float dt);

    void SetRenderItems(const vector<RenderItem*>& renderItems);
    void AddRenderItem(RenderItem* renderItem);

    vector<RenderItem*> GetRenderItems() const;

    void SetPosition(const XMFLOAT3& pos);
    XMFLOAT3 GetPosition() const;
    void SetRotate(const XMFLOAT3& rotate);
    void SetRotateQuat(const XMFLOAT4& rotateQuat);

    spe::Rigidbody* GetRigidbody();
    spe::RigidbodyType GetType()const;
    const spe::AABB GetAABB()const; // debug box

protected:

    void UpdateTransformFromRigidbody();
    void UpdateRenderItem();

protected:

    bool isActive_ = false;

    spe::Rigidbody rigidbody_;
    vector<RenderItem*> renderItems_;

    XMFLOAT3 position_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT4 rotateQuat_ = { 0.0f, 0.0f, 0.0f, 1.0f };
    XMFLOAT3 scale_ = { 1.0f, 1.0f, 1.0f };
};

