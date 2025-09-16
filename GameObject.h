#pragma once

#include "d3dUtil.h"
#include "RenderItem.h"

using namespace std;
using namespace DirectX;

class GameObject
{
public:
    GameObject(const string& name);

    virtual void Update(float dt);

    void SetRenderItems(const vector<RenderItem*>& renderItems);
    void AddRenderItem(RenderItem* renderItem);

    vector<RenderItem*> GetRenderItems() const;

    void UpdateRenderItem();

    void SetPosition(const XMFLOAT3& pos);
    XMFLOAT3 GetPosition() const;

protected:
    XMFLOAT3 position_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 rotation_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 scale_ = { 1.0f, 1.0f, 1.0f };

private:
    string name_;
    vector<RenderItem*> renderItems_;
};

class MovingObject : public GameObject
{
public:
    MovingObject(const string& name);
    virtual void Update(float dt) override;

protected:
    void ApplyForce(const XMFLOAT3& force);
    void UpdatePhysics(float dt);

    float speed_ = 0.0f;

    XMFLOAT3 velocity_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 acceleration_ = { 0.0f, 0.0f, 0.0f };

    float mass_ = 1.0f;
    float drag_ = 0.0f;            // °ø±â ÀúÇ×
    float friction_ = 0.8f;        // ¹Ù´Ú ¸¶Âû °è¼ö
    float gravity_ = 0.98f;
};