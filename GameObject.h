#pragma once

#include "d3dUtil.h"
#include "RenderItem.h"

using namespace std;
using namespace DirectX;

class GameObject
{
public:
    GameObject(const string& name) : name_(name) {}

    virtual void Update(float dt);

    void SetRenderItem(unique_ptr<RenderItem> item) {
        renderItem_ = move(item);
    }

    RenderItem* GetRenderItem() const { return renderItem_.get(); }

    void UpdateRenderItem() {
        XMStoreFloat4x4(&renderItem_->world_,
            XMMatrixScaling(scale_.x, scale_.y, scale_.z) *
            XMMatrixRotationRollPitchYaw(rotation_.x, rotation_.y, rotation_.z) *
            XMMatrixTranslation(position_.x, position_.y, position_.z));
    }

    void SetPosition(const XMFLOAT3& pos) { position_ = pos; }
    XMFLOAT3 GetPosition() const { return position_; }

protected:
    XMFLOAT3 position_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 rotation_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 scale_ = { 1.0f, 1.0f, 1.0f };

private:
    std::string name_;
    std::unique_ptr<RenderItem> renderItem_;
};

class MovingObject : public GameObject
{
public:
    MovingObject(const string& name) : GameObject(name) {}
    virtual void Update(float dt) override;

protected:
    void ApplyForce(const XMFLOAT3& force);
    void UpdatePhysics(float dt);
    float speed_ = 0.0f;
    XMFLOAT3 velocity_ = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 acceleration_ = { 0.0f, 0.0f, 0.0f };
    float mass_ = 1.0f;
    float drag_ = 0.0f;            // °ø±â ÀúÇ×
    float friction_ = 0.1f;        // ¹Ù´Ú ¸¶Âû °è¼ö
    float gravity_ = 0.98f;
};