#include "GameObject.h"

GameObject::GameObject(const XMFLOAT3& scale, const XMFLOAT3& rotate, const XMFLOAT3& position) :
    scale_(scale), rotation_(rotate), position_(position)
{
}

GameObject::GameObject(const XMMATRIX& world)
{
    XMVECTOR s, r, t;
    XMMatrixDecompose(&s, &r, &t, world);

    XMStoreFloat3(&position_, t);
    XMStoreFloat3(&scale_, s);

    XMMATRIX rotM = XMMatrixRotationQuaternion(r);
    rotation_.x = asinf(-rotM.r[2].m128_f32[1]);
    rotation_.y = atan2f(rotM.r[2].m128_f32[0], rotM.r[2].m128_f32[2]);
    rotation_.z = atan2f(rotM.r[0].m128_f32[1], rotM.r[1].m128_f32[1]);
}

void GameObject::Update(float dt)
{


    // update RenderItem
    for (auto& ri : renderItems_)
        if (ri) {
            XMStoreFloat4x4(&ri->world_,
                XMMatrixScaling(scale_.x, scale_.y, scale_.z) *
                XMMatrixRotationRollPitchYaw(rotation_.x, rotation_.y, rotation_.z) *
                XMMatrixTranslation(position_.x, position_.y, position_.z));
            ri->SetFrameDirty();
        }
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

void GameObject::SetRigidBody(const Rigidbody& rigidbody)
{
    rigidbody_ = rigidbody;
    rigidbody_.UpdateBounds();
}

void GameObject::SetRigidBody(uint8_t rigidbodyType, const XMFLOAT3& scale, const XMFLOAT4& rotateQuat, const XMFLOAT3& transform)
{
    rigidbody_.type_ = rigidbodyType;
    rigidbody_.scale_ = scale;
    rigidbody_.orientation_ = rotateQuat;
    rigidbody_.position_ = transform;
    rigidbody_.UpdateBounds();
}

void GameObject::SetRigidBody(uint8_t rigidbodyType, const XMFLOAT3& scale, const XMFLOAT4& rotateQuat, const XMFLOAT3& transform,
    const BoundingOrientedBox& bb, const BoundingSphere& bs)
{
    rigidbody_.type_ = rigidbodyType;
    rigidbody_.scale_ = scale;
    rigidbody_.orientation_ = rotateQuat;
    rigidbody_.position_ = transform;

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
}

XMFLOAT3 GameObject::GetPosition() const 
{
    return position_; 
}

Rigidbody GameObject::GetRigidbody() const
{
    return rigidbody_;
}

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
