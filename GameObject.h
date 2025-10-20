#pragma once

#include "d3dUtil.h"
#include "RenderItem.h"
#include "Rigidbody.h"

using namespace std;
using namespace DirectX;

class GameObject
{
public:
    GameObject() = default;
    GameObject(const XMFLOAT3& scale, const XMFLOAT3& rotate, const XMFLOAT3& position);
    GameObject(const XMMATRIX& world);

    virtual void Update(float dt);

    void SetRenderItems(const vector<RenderItem*>& renderItems);
    void AddRenderItem(RenderItem* renderItem);

    void SetRigidBody(const Rigidbody& rigidbody);
    void SetRigidBody(uint8_t rigidbodyType, const XMFLOAT3& scale, const XMFLOAT4& rotateQuat, const XMFLOAT3& transform);
    void SetRigidBody(uint8_t rigidbodyType, const XMFLOAT3& scale, const XMFLOAT4& rotateQuat, const XMFLOAT3& transform,
        const BoundingOrientedBox& bb, const BoundingSphere& bs);

    vector<RenderItem*> GetRenderItems() const;

    void SetPosition(const XMFLOAT3& pos);
    XMFLOAT3 GetPosition() const;

    Rigidbody GetRigidbody()const;

protected:

    Rigidbody rigidbody_;
    vector<RenderItem*> renderItems_;

    XMFLOAT3 position_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 rotation_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 scale_ = { 1.0f, 1.0f, 1.0f };
};

class MovingObject : public GameObject
{
public:
    MovingObject();
    virtual void Update(float dt) override;

    bool IsAccelerating() const;
protected:
    void ApplyForce(const XMFLOAT3& force);
    void UpdatePhysics(float dt);

    float speed_ = 0.0f;
    
    XMFLOAT3 velocity_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 acceleration_ = { 0.0f, 0.0f, 0.0f };

    float mass_ = 1.0f;
    float drag_ = 0.0f;            // °ø±â ÀúÇ×
    float friction_ = 0.7f;        // ¹Ù´Ú ¸¶Âû °è¼ö
    float linearFriction_ = 3.0f;

    bool hasGravity = true;
    float gravity_ = 9.8f;
};