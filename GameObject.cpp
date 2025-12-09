#include "GameObject.h"

GameObject::GameObject(const spe::RigidbodyType rigidbodyType) :
    scale_(XMFLOAT3(1.0f, 1.0f, 1.0f)), position_(XMFLOAT3(0.0f, 0.0f, 0.0f)), rotateQuat_(XMFLOAT4(0.f, 0.f, 0.f, 1.f))
{   
    rigidbody_ = spe::Rigidbody(rigidbodyType, rotateQuat_, position_);
}

GameObject::GameObject(const XMFLOAT3& scale, const XMFLOAT3& rotate, const XMFLOAT3& position,
    const spe::RigidbodyType rigidbodyType = spe::RigidbodyType::STATIC) :
    scale_(scale), position_(position)
{
    XMStoreFloat4(&rotateQuat_, XMQuaternionRotationRollPitchYaw(rotate.x, rotate.y, rotate.z));
    rigidbody_ = spe::Rigidbody(rigidbodyType, rotateQuat_, position);
    rigidbody_.SetGameObject(this);
}

void GameObject::OnSpawn()
{ 
    isActive_ = true;
}

void GameObject::OnDestroy()
{ 
    isActive_ = false;
}

bool GameObject::isActive() const
{
    return isActive_;
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

void GameObject::SetRotate(const XMFLOAT3& rotate)
{
    XMStoreFloat4(&rotateQuat_, XMQuaternionRotationRollPitchYaw(rotate.x, rotate.y, rotate.z));
    rigidbody_.orientation_ = rotateQuat_;
}

void GameObject::SetRotateQuat(const XMFLOAT4& rotateQuat)
{
    rotateQuat_ = rotateQuat;
    rigidbody_.orientation_ = rotateQuat_;
}

spe::Rigidbody* GameObject::GetRigidbody()
{
    return &rigidbody_;
}

spe::RigidbodyType GameObject::GetType()const
{
    return rigidbody_.GetType();
}

const spe::AABB GameObject::GetAABB() const
{
    XMMATRIX xf = rigidbody_.GetTransformMatrix();
    return rigidbody_.fixture_->GetShape()->GetAABB(xf);
}

void GameObject::UpdateTransformFromRigidbody()
{
    // update pos to rigidbody
    rotateQuat_ = rigidbody_.orientation_;
    position_ = rigidbody_.position_;
}

void GameObject::UpdateRenderItem()
{
    // update RenderItem
    for (auto ri : renderItems_)
        if (ri != nullptr) {
            XMStoreFloat4x4(&ri->world_,
                XMMatrixAffineTransformation(
                    XMLoadFloat3(&scale_),
                    XMVectorZero(),
                    XMLoadFloat4(&rotateQuat_),
                    XMLoadFloat3(&position_)));
            ri->SetFrameDirty();
        }
}
