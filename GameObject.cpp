#include "GameObject.h"

GameObject::GameObject(const XMFLOAT3& scale, const XMFLOAT3& rotate, const XMFLOAT3& position, 
    const uint8_t rigidbodyType = (uint8_t)RigidbodyType::Static) :
    scale_(scale), position_(position)
{
    XMStoreFloat4(&rotateQuat_, XMQuaternionRotationRollPitchYaw(rotate.x, rotate.y, rotate.z));
    rigidbody_ = Rigidbody(rigidbodyType, scale, rotateQuat_, position);
    rigidbody_.UpdateBounds();
}

void GameObject::Update(float dt)
{
    UpdateTransformFromRigidbody();
    UpdateRenderItem();
}

void GameObject::SetRenderItems(const vector<RenderItem*>& renderItems)
{
    for(auto ri : renderItems)
    renderItems_.push_back(ri);
}

void GameObject::AddRenderItem(RenderItem* renderItem)
{
    renderItems_.push_back(renderItem);
}

void GameObject::SetBounds(const BoundingOrientedBox& bb, const BoundingSphere& bs)
{
    rigidbody_.localBoundingBox_ = bb;
    rigidbody_.localBoundingSphere_ = bs;
    rigidbody_.UpdateBounds();
}

vector<RenderItem*> GameObject::GetRenderItems() const 
{ 
    return renderItems_; 
}

void GameObject::SetPosition(const XMFLOAT3& pos) 
{ 
    position_ = pos; 
    rigidbody_.position_ = pos;
}

XMFLOAT3 GameObject::GetPosition() const 
{
    return position_; 
}

Rigidbody& GameObject::GetRigidbody()
{
    return rigidbody_;
}

void GameObject::UpdateTransformFromRigidbody()
{
    // update pos to rigidbody
    scale_ = rigidbody_.scale_;
    rotateQuat_ = rigidbody_.orientation_;
    position_ = rigidbody_.position_;
}

void GameObject::UpdateRenderItem()
{
    // update RenderItem
    for (auto& ri : renderItems_)
        if (ri) {
            XMStoreFloat4x4(&ri->world_,
                XMMatrixAffineTransformation(
                    XMLoadFloat3(&scale_),
                    XMVectorZero(),
                    XMLoadFloat4(&rotateQuat_),
                    XMLoadFloat3(&position_)));
            ri->SetFrameDirty();
        }
}

/*
MovingObject::MovingObject() : GameObject()
{
}

void MovingObject::Update(float dt)
{
    UpdatePhysics(dt);
}

bool MovingObject::IsAccelerating() const
{
    if (fabs(acceleration_.x) <= MathHelper::EPS
        && fabs(acceleration_.y) <= MathHelper::EPS
        && fabs(acceleration_.z) <= MathHelper::EPS)
        return false;
    return true;
}

void MovingObject::ApplyForce(const XMFLOAT3& force)
{
    acceleration_.x += force.x;
    acceleration_.y += force.y;
    acceleration_.z += force.z;
}

void MovingObject::UpdatePhysics(float dt)
{
    velocity_.x += acceleration_.x * dt;
    velocity_.y += acceleration_.y * dt;
    velocity_.z += acceleration_.z * dt;

    if (hasGravity && position_.y >= 0.0f)
        velocity_.y -= gravity_ * dt;

    acceleration_ = { 0.0f, 0.0f, 0.0f };

    velocity_.x *= powf(1.0f - friction_, dt);
    velocity_.y *= powf(1.0f - friction_, dt);
    velocity_.z *= powf(1.0f - friction_, dt);

    float lf = linearFriction_ * dt;
    if (velocity_.x > 0.0f) {
        velocity_.x -= lf;
        if (velocity_.x < 0.0f) velocity_.x = 0.0f;
    }
    else if (velocity_.x < 0.0f) {
        velocity_.x += lf;
        if (velocity_.x > 0.0f) velocity_.x = 0.0f;
    }
    if (velocity_.z > 0.0f) {
        velocity_.z -= lf;
        if (velocity_.z < 0.0f) velocity_.z = 0.0f;
    }
    else if (velocity_.z < 0.0f) {
        velocity_.z += lf;
        if (velocity_.z > 0.0f) velocity_.z = 0.0f;
    }

    if (fabs(velocity_.x) + fabs(velocity_.z) < 0.01f) {
        velocity_.x = 0.0f;
        velocity_.z = 0.0f;
    }

    position_.x += velocity_.x * dt;
    position_.y += velocity_.y * dt;
    position_.z += velocity_.z * dt;

    if (position_.y <= 0.0f) {
        position_.y = 0.0f;
        velocity_.y = 0.0f;
    }
}
*/